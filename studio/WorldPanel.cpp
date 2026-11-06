#include "WorldPanel.hpp"
#include "RhiBackend.hpp"
#include "TileTexture.hpp"
#include "world/HdSurface.hpp"
#include "world/WorldMapHelpers.hpp"
#include "world/MapView.hpp"
#include "world/TerrainRhiWidget.hpp"
#include "world/WorldWeatherOverlay.hpp"
#include "world/WorldClimateOverlays.hpp"
#include "world/WorldMarkerOverlays.hpp"
#include "world/WorldCamera.hpp"
#include "aur/autotile.hpp"
#include "aur/clock.hpp"
#include "aur/forge.hpp"
#include "aur/game.hpp"
#include "aur/world_topology.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressDialog>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <queue>
#include <cstring>
#include <utility>

namespace aur {

class WorldMapOverlayWidget final : public QWidget {
public:
    explicit WorldMapOverlayWidget(WorldMapCanvasWidget* owner)
        : QWidget(owner), owner_(owner) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        owner_->paintCanvas(painter);
    }

private:
    WorldMapCanvasWidget* owner_;
};

WorldMapCanvasWidget::WorldMapCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 320);
    setMouseTracking(true);
    setAutoFillBackground(false);
    terrain_renderer = new TerrainRhiWidget([this]() { return terrainView(); }, this);
    overlay_widget = new WorldMapOverlayWidget(this);
    terrain_renderer->hide();
    terrain_renderer->setGeometry(rect());
    overlay_widget->setGeometry(rect());
    overlay_widget->raise();
    simulation_timer.setInterval(100);
    connect(&simulation_timer, &QTimer::timeout, this, [this]() {
        ++simulation_phase;
        if (simulation_phase % 100 == 0) {
            preview_hour = std::fmod(preview_hour + 0.25, 24.0);
            emit timeOfDayChanged(static_cast<int>(preview_hour),
                                  static_cast<int>((preview_hour -
                                      std::floor(preview_hour)) * 60.0));
        }
        refreshCanvas();
    });
}

WorldMapCanvasWidget::~WorldMapCanvasWidget() = default;

void WorldMapCanvasWidget::setProject(Project* p) {
    project = p;
    history.clear();
    tile_cache.clear();
    world_model.reset();
    world_model_map.clear();
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::setMap(const std::string& map_id) {
    if (current_map != map_id) {
        history.clear();
        current_map = map_id;
        world_model.reset();
        world_model_map.clear();
        selected_node = -1;
        hover_tile = {-1, -1};
        pan_offset = {};
        tile_cache.clear();
        surface_cache = {};
        surface_cache_map.clear();
    }
    refreshCanvas();
}

void WorldMapCanvasWidget::ensureWorldModel() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !world::has_hd_surface(project->maps.at(current_map))) {
        world_model.reset();
        world_model_map.clear();
        return;
    }
    if (world_model && world_model_map == current_map) return;
    world_model = std::make_unique<editor::WorldModel>(
        editor::WorldModel::fromMap(project->maps.at(current_map), project->biomes));
    world_model_map = current_map;
    ++world_model_generation;
}

void WorldMapCanvasWidget::refreshCanvas() {
    ensureWorldModel();
    const bool gpuTerrain = world_model && terrain_renderer && !editor::isHeadlessRhi();
    if (terrain_renderer) {
        terrain_renderer->setVisible(gpuTerrain);
        if (gpuTerrain) terrain_renderer->update();
    }
    if (overlay_widget) overlay_widget->update();
    QWidget::update();
}

void WorldMapCanvasWidget::setTool(WorldTool tool) {
    selected_tool = tool;
    setCursor(selected_tool != WorldTool::Inspect ? Qt::CrossCursor : Qt::ArrowCursor);
}

void WorldMapCanvasWidget::setSelectedTile(int tile_index) {
    selected_tile = tile_index;
}

void WorldMapCanvasWidget::setBrushSize(int size) {
    brush_size = std::clamp(size, 1, 15);
}

void WorldMapCanvasWidget::setSelectedBiome(const std::string& biome_id) {
    selected_biome = biome_id;
}

void WorldMapCanvasWidget::setSelectedNode(int node_index) {
    selected_node = node_index;
    refreshCanvas();
}

void WorldMapCanvasWidget::setOverlay(WorldOverlay overlay, bool enabled) {
    switch (overlay) {
        case WorldOverlay::Temperature: temperature_overlay = enabled; break;
        case WorldOverlay::Wind: wind_overlay = enabled; break;
        case WorldOverlay::Currents: currents_overlay = enabled; break;
        case WorldOverlay::Weather: weather_overlay = enabled; break;
        case WorldOverlay::DayNight: daynight_overlay = enabled; break;
        case WorldOverlay::None: break;
    }
    refreshCanvas();
}

void WorldMapCanvasWidget::setWeatherType(const std::string& weather) {
    preview_weather = weather;
    refreshCanvas();
}

void WorldMapCanvasWidget::setSimulationRunning(bool enabled) {
    if (enabled) simulation_timer.start();
    else simulation_timer.stop();
    refreshCanvas();
}

void WorldMapCanvasWidget::setContoursEnabled(bool enabled) {
    contours_enabled = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setTimeOfDay(int hour, int minute) {
    preview_hour = std::clamp(hour, 0, 23) + std::clamp(minute, 0, 59) / 60.0;
    refreshCanvas();
}

namespace {

studio::WorldCameraSpec cameraSpec(const Project* project, const std::string& map_id,
                                   const QWidget* widget, qreal zoom, QPointF pan) {
    studio::WorldCameraSpec spec;
    const auto map_view = world::MapView::current(const_cast<Project*>(project), map_id);
    if (!map_view) return spec;
    spec.map_width = map_view->width();
    spec.map_height = map_view->height();
    spec.wraps_x = map_view->wrapsX();
    spec.zoom = zoom;
    spec.pan = pan;
    spec.viewport = widget->size();
    if (!map_view->hasHdSurface()) {
        const auto tileset = project->tilesets.find(map_view->tileset());
        spec.source_tile_size = tileset != project->tilesets.end() &&
            tileset->second.contains("tile") ? tileset->second["tile"].as_int() : 16;
    }
    return spec;
}

} // namespace

void WorldMapCanvasWidget::setZoom(qreal zoom) {
    zoom_factor = studio::WorldCamera::clampZoom(zoom);
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
}

void WorldMapCanvasWidget::fitToView() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset);
    pan_offset = {};
    setZoom(studio::WorldCamera::fitZoom(spec, size()));
}

void WorldMapCanvasWidget::recordEntityEdit() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) map["entities"] = JsonArray{};
    history.record_entity(map["entities"]);
}

QRectF WorldMapCanvasWidget::mapBounds() const {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return {};
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset);
    return studio::WorldCamera::mapBounds(spec, size());
}

void WorldMapCanvasWidget::constrainWorldCamera() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset);
    studio::WorldCameraState state{zoom_factor, pan_offset};
    studio::WorldCamera::constrain(state, spec);
    pan_offset = state.pan;
}

QPoint WorldMapCanvasWidget::tileAt(const QPointF& position) const {
    if (!project || !project->maps.contains(current_map)) return {-1, -1};
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset);
    return studio::WorldCamera::tileAt(spec, position);
}

QImage WorldMapCanvasWidget::tileImage(const std::string& tileset_id, int tile_index) {
    const auto key = std::make_pair(tileset_id, tile_index);
    const auto cached = tile_cache.find(key);
    if (cached != tile_cache.end()) return cached->second;

    const QImage image = make_tileset_tile(*project, tileset_id, tile_index);
    return tile_cache.emplace(key, image).first->second;
}

TerrainView WorldMapCanvasWidget::terrainView() const {
    return {world_model.get(), world_model_map, world_model_generation, project,
            mapBounds(), size(), zoom_factor, preview_hour, daynight_overlay,
            contours_enabled};
}

void WorldMapCanvasWidget::paintTile(const QPoint& tile) {
    if (!world::is_paint_tool(selected_tool) && !world::is_height_tool(selected_tool)) return;
    if (!project || tile.y() < 0 ||
        !project->maps.contains(current_map)) return;

    Json& map = project->maps[current_map];
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    const bool wraps_x = aur::wraps_world_x(map);
    if (tile.y() >= map_height ||
        (!wraps_x && (tile.x() < 0 || tile.x() >= map_width))) return;
    const int center_x = wraps_x ? aur::wrapX(tile.x(), map_width) : tile.x();
    if (!map.contains("layers") || !map["layers"].is_object()) return;
    const bool hdSurface = world::has_hd_surface(map);
    const int radius = brush_size / 2;
    std::vector<QPoint> stamp;
    std::set<int> stamp_indices;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (radius > 0 && dx * dx + dy * dy > radius * radius + radius) continue;
            const int y = tile.y() + dy;
            if (y < 0 || y >= map_height) continue;
            const int x = wraps_x ? aur::wrapX(center_x + dx, map_width) : center_x + dx;
            if (x < 0 || x >= map_width) continue;
            const int index = y * map_width + x;
            if (stamp_indices.insert(index).second) stamp.emplace_back(x, y);
        }
    }
    if (world::is_height_tool(selected_tool)) {
        ensureWorldModel();
        if (!world_model) return;
        constexpr int heightStep = 20;
        for (const QPoint& position : stamp) {
            const int index = position.y() * world_model->width() + position.x();
            if (stroke.active && stroke.captured_cells.insert(index).second) {
                stroke.model_rects.push_back(world_model->captureRect(
                    editor::WorldModel::Layer::Height, {position.x(), position.y(), 1, 1}));
            }
            const std::uint16_t oldHeight =
                world_model->heights()[static_cast<std::size_t>(index)];
            const int delta = selected_tool == WorldTool::RaiseHeight ? heightStep : -heightStep;
            const std::uint16_t newHeight = static_cast<std::uint16_t>(
                std::clamp(static_cast<int>(oldHeight) + delta, 0, 65535));
            const std::array<std::uint16_t, 1> value{newHeight};
            const editor::WorldModel::Rect rect{position.x(), position.y(), 1, 1};
            world_model->setRect(editor::WorldModel::Layer::Height, rect, value);
            world_model->writeRect(map, editor::WorldModel::Layer::Height, rect);
        }
        refreshCanvas();
        return;
    }

    if (selected_tool == WorldTool::PaintTerrain) {
        if (!hdSurface && !map["layers"].contains("ground")) return;
        if (hdSurface) {
            const int map_width = map["w"].as_int();
            const int map_height = map["h"].as_int();
            if (!map["layers"].contains("terrain_overrides") ||
                !map["layers"]["terrain_overrides"].is_array()) {
                map["layers"]["terrain_overrides"] = JsonArray{};
            }
            JsonArray& overrides = map["layers"]["terrain_overrides"].as_array();
            while (static_cast<int>(overrides.size()) < map_height) {
                JsonArray row;
                for (int x = 0; x < map_width; ++x) row.emplace_back(-1);
                overrides.emplace_back(std::move(row));
            }
            for (int y = 0; y < map_height; ++y) {
                Json& row_value = overrides[static_cast<size_t>(y)];
                if (!row_value.is_array()) {
                    JsonArray row;
                    for (int x = 0; x < map_width; ++x) row.emplace_back(-1);
                    row_value = std::move(row);
                    continue;
                }
                JsonArray& row = row_value.as_array();
                while (static_cast<int>(row.size()) < map_width) row.emplace_back(-1);
            }
        }
        const auto tileset_it = project->tilesets.find(map["tileset"].as_string());
        const Json* tileset = tileset_it == project->tilesets.end()
            ? nullptr : &tileset_it->second;
        const std::map<int, std::string> groups = tileset ? tile_groups(*tileset)
                                                         : std::map<int, std::string>{};
        const auto selected_group = groups.find(selected_tile);
        for (const QPoint& position : stamp) {
            if (hdSurface || (map["layers"].contains("terrain_overrides") &&
                map["layers"]["terrain_overrides"].is_array())) {
                map["layers"]["terrain_overrides"][static_cast<size_t>(position.y())]
                    [static_cast<size_t>(position.x())] = selected_tile;
            }
            if (hdSurface) continue;
            if (selected_group != groups.end() && tileset) {
                aur::paint(map, *tileset, "ground", position.x(), position.y(),
                            selected_group->second);
            } else {
                map["layers"]["ground"][static_cast<size_t>(position.y())]
                    [static_cast<size_t>(position.x())] = selected_tile;
                if (tileset) {
                    for (const auto& [dx, dy] :
                         std::array<std::pair<int, int>, 5>{{{0, 0}, {0, -1}, {1, 0}, {0, 1}, {-1, 0}}}) {
                        retile(map, *tileset, "ground", position.x() + dx,
                               position.y() + dy, groups);
                    }
                }
            }
        }
    } else if (hdSurface) {
        ensureWorldModel();
        if (!world_model) return;
        const auto biomeId = world_model->biomeId(selected_biome);
        if (!biomeId) {
            QMessageBox::warning(this, "Biome paint failed",
                                 "This map uses more than 32 biome colors; the selected biome "
                                 "cannot be represented by the sculpt palette.");
            return;
        }
        for (const QPoint& position : stamp) {
            const int index = position.y() * world_model->width() + position.x();
            if (stroke.active && stroke.captured_cells.insert(index).second) {
                stroke.model_rects.push_back(world_model->captureRect(
                    editor::WorldModel::Layer::Biome, {position.x(), position.y(), 1, 1}));
            }
            const std::array<std::uint8_t, 1> value{*biomeId};
            const editor::WorldModel::Rect rect{position.x(), position.y(), 1, 1};
            world_model->setRect(editor::WorldModel::Layer::Biome, rect, value);
            world_model->writeRect(map, editor::WorldModel::Layer::Biome, rect);
        }
    } else {
        try {
            for (const QPoint& position : stamp) {
                paint_biome(*project, current_map, position.x(), position.y(), selected_biome);
            }
        } catch (const std::exception& error) {
            QMessageBox::warning(this, "Biome paint failed", QString::fromStdString(error.what()));
            setTool(WorldTool::Inspect);
            return;
        }
    }
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::undo() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !history.can_undo()) return;
    Json& map = project->maps[current_map];
    const editor::WorldCommandHistory::Action action = history.take_undo();
    if (action.entities) {
        editor::WorldCommandHistory::Action redo;
        redo.entities = true;
        redo.state = map["entities"];
        history.push_redo(std::move(redo));
        map["entities"] = action.state;
        emit entitiesChanged();
    } else if (!action.model_rects.empty() && world_model) {
        editor::WorldCommandHistory::Action redo;
        for (auto it = action.model_rects.rbegin(); it != action.model_rects.rend(); ++it) {
            redo.model_rects.push_back(world_model->captureRect(it->layer, it->rect));
            world_model->restore(*it);
            world_model->writeRect(map, it->layer, it->rect);
        }
        history.push_redo(std::move(redo));
    } else {
        editor::WorldCommandHistory::Action redo;
        redo.entities = false;
        redo.state = map["layers"];
        history.push_redo(std::move(redo));
        map["layers"] = action.state;
    }
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::redo() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !history.can_redo()) return;
    Json& map = project->maps[current_map];
    const editor::WorldCommandHistory::Action action = history.take_redo();
    if (action.entities) {
        editor::WorldCommandHistory::Action undo;
        undo.entities = true;
        undo.state = map["entities"];
        history.push_undo(std::move(undo));
        map["entities"] = action.state;
        emit entitiesChanged();
    } else if (!action.model_rects.empty() && world_model) {
        editor::WorldCommandHistory::Action undo;
        for (auto it = action.model_rects.rbegin(); it != action.model_rects.rend(); ++it) {
            undo.model_rects.push_back(world_model->captureRect(it->layer, it->rect));
            world_model->restore(*it);
            world_model->writeRect(map, it->layer, it->rect);
        }
        history.push_undo(std::move(undo));
    } else {
        editor::WorldCommandHistory::Action undo;
        undo.entities = false;
        undo.state = map["layers"];
        history.push_undo(std::move(undo));
        map["layers"] = action.state;
    }
    surface_cache = {};
    surface_cache_map.clear();
    update();
}

void WorldMapCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#171b20"));
}

void WorldMapCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (terrain_renderer) terrain_renderer->setGeometry(rect());
    if (overlay_widget) overlay_widget->setGeometry(rect());
    refreshCanvas();
}

void WorldMapCanvasWidget::paintCanvas(QPainter& painter) {
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const bool gpuTerrain = world_model && terrain_renderer && terrain_renderer->isVisible();
    if (!gpuTerrain) painter.fillRect(rect(), QColor("#171b20"));

    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        painter.setPen(QColor("#aeb8c2"));
        painter.drawText(rect(), Qt::AlignCenter, "Select a world map to begin");
        return;
    }

    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    const QRectF bounds = mapBounds();
    const bool hd_surface = world::has_hd_surface(map);
    const bool wraps_x = aur::wraps_world_x(map);
    if (bounds.isEmpty() || !map.contains("layers") || !map["layers"].is_object() ||
        (!hd_surface && !map["layers"].contains("ground"))) return;

    const JsonArray* ground = !hd_surface && map["layers"]["ground"].is_array()
        ? &map["layers"]["ground"].as_array() : nullptr;
    const qreal cell = bounds.width() / map_width;
    const std::string tileset_id = map["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    const Json* tileset = tileset_it == project->tilesets.end()
        ? nullptr : &tileset_it->second;
    const std::map<int, std::string> groups = tileset ? tile_groups(*tileset)
                                                     : std::map<int, std::string>{};
    const auto is_ocean_tile = [&groups](int tile_index) {
        const auto group = groups.find(tile_index);
        return group != groups.end() && group->second == "ocean";
    };
    const JsonArray* elevation_rows = map["layers"].contains("elevation") &&
        map["layers"]["elevation"].is_array()
        ? &map["layers"]["elevation"].as_array() : nullptr;
    const int sea_level = map.contains("worldgen") &&
        map["worldgen"].contains("sea_level")
        ? map["worldgen"]["sea_level"].as_int() : 500;
    const auto is_ocean_at = [&](int x, int y) {
        if (wraps_x) x = aur::wrapX(x, map_width);
        if (hd_surface && elevation_rows && y >= 0 &&
            y < static_cast<int>(elevation_rows->size())) {
            const Json& row = (*elevation_rows)[static_cast<size_t>(y)];
            if (row.is_array() && x >= 0 &&
                x < static_cast<int>(row.as_array().size())) {
                return row[static_cast<size_t>(x)].as_int() <= sea_level;
            }
        }
        if (!ground || y < 0 || y >= static_cast<int>(ground->size())) return false;
        const Json& row = (*ground)[static_cast<size_t>(y)];
        return row.is_array() && x >= 0 &&
            x < static_cast<int>(row.as_array().size()) &&
            is_ocean_tile(row[static_cast<size_t>(x)].as_int());
    };
    const int first_visible_x =
        static_cast<int>(std::floor((0.0 - bounds.left()) / cell));
    const int last_visible_x =
        static_cast<int>(std::ceil((width() - bounds.left()) / cell));
    const int first_x = wraps_x ? first_visible_x
        : std::clamp(first_visible_x, 0, map_width);
    const int last_x = wraps_x ? last_visible_x
        : std::clamp(last_visible_x, 0, map_width);
    const int first_y = std::clamp(static_cast<int>(std::floor((0.0 - bounds.top()) / cell)),
                                   0, map_height);
    const int last_y = std::clamp(static_cast<int>(std::ceil((height() - bounds.top()) / cell)),
                                  0, map_height);

    if (!gpuTerrain) painter.fillRect(bounds.adjusted(-1, -1, 1, 1), QColor("#080a0c"));
    if (hd_surface) {
        if (!gpuTerrain) {
            if (surface_cache_map != current_map || surface_cache.isNull()) {
                surface_cache = world::build_hd_surface(map, project, QRect());
                surface_cache_map = current_map;
            }
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            if (wraps_x) {
                const int first_copy = static_cast<int>(
                    std::floor((0.0 - bounds.left()) / bounds.width()));
                const int last_copy = static_cast<int>(
                    std::ceil((width() - bounds.left()) / bounds.width()));
                for (int copy = first_copy; copy < last_copy; ++copy) {
                    QRectF repeated_bounds = bounds;
                    repeated_bounds.moveLeft(bounds.left() + copy * bounds.width());
                    painter.drawImage(repeated_bounds, surface_cache);
                }
            } else {
                painter.drawImage(bounds, surface_cache);
            }
            painter.restore();
        }
        if (map["layers"].contains("terrain_overrides") &&
            map["layers"]["terrain_overrides"].is_array()) {
            const auto& overrides = map["layers"]["terrain_overrides"].as_array();
            for (int y = first_y; y < last_y && y < static_cast<int>(overrides.size()); ++y) {
                const Json& row = overrides[static_cast<size_t>(y)];
                if (!row.is_array()) continue;
                for (int x = first_x; x < last_x; ++x) {
                    const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                    if (source_x < 0 || source_x >= static_cast<int>(row.as_array().size())) continue;
                    const int tile_index = row[static_cast<size_t>(source_x)].as_int();
                    if (tile_index < 0) continue;
                    const QRectF tile_rect(bounds.left() + x * cell, bounds.top() + y * cell,
                                           cell, cell);
                    const QImage tile = tileImage(tileset_id, tile_index);
                    if (!tile.isNull()) painter.drawImage(tile_rect, tile);
                }
            }
        }
    } else {
        if (!ground) return;
        for (int y = first_y; y < last_y && y < static_cast<int>(ground->size()); ++y) {
            const auto& row = (*ground)[static_cast<size_t>(y)].as_array();
            for (int x = first_x; x < last_x; ++x) {
                const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                if (source_x < 0 || source_x >= static_cast<int>(row.size())) continue;
                int tile_index = row[static_cast<size_t>(source_x)].as_int();
                if (tileset && groups.contains(tile_index) && groups.at(tile_index) == "ocean") {
                    const int mask = mask_at(map, *tileset, "ground", source_x, y, "ocean", groups);
                    const auto& ocean_tiles = (*tileset)["autotiles"]["ocean"]["tiles"].as_array();
                    if (mask >= 0 && static_cast<size_t>(mask) < ocean_tiles.size()) {
                        tile_index = ocean_tiles[static_cast<size_t>(mask)].as_int();
                    }
                }
                const QRectF tile_rect(bounds.left() + x * cell, bounds.top() + y * cell,
                                       cell, cell);
                const QImage tile = tileImage(tileset_id, tile_index);
                if (!tile.isNull()) painter.drawImage(tile_rect, tile);
                else painter.fillRect(tile_rect, QColor("#34404a"));
            }
        }
    }

    const JsonArray* biome_rows = map["layers"].contains("biomes") &&
        map["layers"]["biomes"].is_array()
        ? &map["layers"]["biomes"].as_array() : nullptr;
    const JsonArray* temperature_rows = map["layers"].contains("temperature") &&
        map["layers"]["temperature"].is_array()
        ? &map["layers"]["temperature"].as_array() : nullptr;

    if (!hd_surface && biome_rows) {
        const auto& rows = *biome_rows;
        for (int y = first_y; y < last_y && y < static_cast<int>(rows.size()); ++y) {
            const Json& row = rows[static_cast<size_t>(y)];
            if (!row.is_array()) continue;
            for (int x = first_x; x < last_x; ++x) {
                const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                if (source_x < 0 || source_x >= static_cast<int>(row.as_array().size())) continue;
                const std::string biome_id = row[static_cast<size_t>(source_x)].as_string();
                const auto biome_it = project->biomes.find(biome_id);
                if (biome_it == project->biomes.end() || !biome_it->second["color"].is_string()) continue;
                QColor color(QString::fromStdString(biome_it->second["color"].as_string()));
                if (!color.isValid()) continue;
                color.setAlpha(38);
                painter.fillRect(QRectF(bounds.left() + x * cell, bounds.top() + y * cell, cell, cell),
                                 color);
            }
        }
    }

    if (temperature_overlay && temperature_rows) {
        world::TemperatureOverlayContext temperature_ctx{painter, project, temperature_rows,
            bounds, cell, map_width, first_x, last_x, first_y, last_y, wraps_x};
        world::draw_temperature_overlay(temperature_ctx);
    }

    if (wind_overlay || currents_overlay) {
        world::ClimateAxesContext axes_ctx{painter, project, &map, bounds, QRectF(rect()), cell, wraps_x};
        world::draw_climate_axes(axes_ctx);

        world::FlowOverlayContext flow_ctx{painter, project, &map, bounds, cell, map_width, map_height,
            first_x, last_x, first_y, last_y, wraps_x, currents_overlay};
        world::draw_flow_overlay(flow_ctx);
    }

    const int worldgen_seed = map.contains("worldgen") &&
        map["worldgen"].contains("seed") ? map["worldgen"]["seed"].as_int()
        : map.contains("seed") ? map["seed"].as_int() : 1;
    const int axis_degrees = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt") ? map["worldgen"]["axial_tilt"].as_int() : 0;
    const qreal axis = axis_degrees * 3.14159265358979323846 / 180.0;
    const qreal axis_sine = std::sin(axis);
    const qreal axis_cosine = std::cos(axis);
    const qreal latitude_scale = std::max<qreal>(
        0.001, std::abs(axis_sine) + std::abs(axis_cosine));
    const qreal east_x = wraps_x ? 1.0 : axis_cosine;
    const qreal east_y = wraps_x ? 0.0 : -axis_sine;
    const qreal north_x = wraps_x ? 0.0 : axis_sine;
    const qreal north_y = wraps_x ? 1.0 : axis_cosine;

    if ((wind_overlay || currents_overlay) && cell > 0) {
        world::ClimateAxesContext axes_ctx{painter, project, &map, bounds, QRectF(rect()), cell, wraps_x};
        world::draw_climate_axes(axes_ctx);

        world::FlowOverlayContext flow_ctx{painter, project, &map, bounds, cell, map_width, map_height,
            first_x, last_x, first_y, last_y, wraps_x, currents_overlay,
            simulation_phase, simulation_timer.isActive(), is_ocean_at};
        world::draw_flow_overlay(flow_ctx);
    }

    if (weather_overlay && cell > 0) {
        world::WeatherOverlayContext weather_ctx{painter, project,
            temperature_rows, biome_rows, bounds, QRectF(rect()), cell,
            map_width, map_height, first_x, last_x, wraps_x, worldgen_seed,
            simulation_phase, simulation_timer.isActive(), preview_hour,
            preview_weather, is_ocean_at};
        world::draw_weather_overlay(weather_ctx);
    }

    if (daynight_overlay && (!world_model || editor::isHeadlessRhi())) {
        world::DayNightContext daynight_ctx{painter, project, bounds, QRectF(rect()), wraps_x, preview_hour};
        world::draw_daynight_fallback(daynight_ctx);
    }

    world::EntityMarkersContext markers_ctx{painter, project,
        map.contains("entities") && map["entities"].is_array()
            ? &map["entities"].as_array() : nullptr,
        bounds, QRectF(rect()), cell, zoom_factor, map_width, map_height,
        selected_node, wraps_x};
    world::draw_entity_markers(markers_ctx);

    world::HoverContext hover_ctx{painter, bounds, cell, hover_tile.x(),
        hover_tile.y(), map_width, map_height, static_cast<qreal>(width()), wraps_x};
    world::draw_hover_highlight(hover_ctx);

    world::ZoomLabelContext zoom_ctx{painter, hd_surface, map_width, map_height,
        map["tileset"].as_string(), zoom_factor, static_cast<qreal>(width())};
    world::draw_zoom_label(zoom_ctx);
}

void WorldMapCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning = true;
        pan_start_position = event->position();
        pan_start_offset = pan_offset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        stroke.end();
        const QPoint tile = tileAt(event->position());
        if (world::is_entity_placement(selected_tool) &&
            tile.x() >= 0 && tile.y() >= 0) {
            emit entityPlacementRequested(tile, selected_tool);
            event->accept();
            return;
        }
        if (selected_tool == WorldTool::Inspect && project && project->maps.contains(current_map) &&
            tile.x() >= 0 && tile.y() >= 0) {
            const Json& map = project->maps.at(current_map);
            if (!map.contains("entities") || !map["entities"].is_array()) {
                emit entitySelected(-1);
                return;
            }
            const auto& entities = map["entities"].as_array();
            int nearest = -1;
            qreal nearest_distance = std::numeric_limits<qreal>::max();
            for (size_t index = 0; index < entities.size(); ++index) {
                const Json& entity = entities[index];
                const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
                if (type != "location" && type != "exit") continue;
                const qreal dx = entity["x"].as_int() - tile.x();
                const qreal dy = entity["y"].as_int() - tile.y();
                const qreal distance = std::hypot(dx, dy);
                if (distance <= 1.5 && distance < nearest_distance) {
                    nearest = static_cast<int>(index);
                    nearest_distance = distance;
                }
            }
            emit entitySelected(nearest);
        }
        const bool modelStroke = world::is_height_tool(selected_tool) ||
            (selected_tool == WorldTool::PaintBiome && project && project->maps.contains(current_map) &&
             world::has_hd_surface(project->maps.at(current_map)));
        if ((selected_tool == WorldTool::PaintTerrain ||
             (selected_tool == WorldTool::PaintBiome && !modelStroke)) && project &&
            project->maps.contains(current_map) && tile.x() >= 0 && tile.y() >= 0) {
            history.record_layers(project->maps[current_map]["layers"]);
        }
        if ((world::is_paint_tool(selected_tool) || modelStroke) &&
            project && project->maps.contains(current_map) && tile.x() >= 0 && tile.y() >= 0) {
            stroke.begin(tile);
        }
        paintTile(tile);
    }
}

void WorldMapCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning && event->buttons().testFlag(Qt::MiddleButton)) {
        pan_offset = pan_start_offset + event->position() - pan_start_position;
        constrainWorldCamera();
        refreshCanvas();
        return;
    }
    const QPoint tile = tileAt(event->position());
    if (tile != hover_tile) {
        hover_tile = tile;
        refreshCanvas();
    }
    if (stroke.active && event->buttons().testFlag(Qt::LeftButton) &&
        tile.x() >= 0 && tile.y() >= 0 && tile != stroke.last_tile) {
        const bool wraps_x = project && project->maps.contains(current_map) &&
            aur::wraps_world_x(project->maps.at(current_map));
        const int wrap_width = wraps_x
            ? project->maps.at(current_map)["w"].as_int() : 0;
        for (const QPoint& point : world::stroke_line_tiles(stroke.last_tile, tile, wrap_width)) {
            paintTile(point);
        }
        stroke.last_tile = tile;
    }
}

void WorldMapCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (!stroke.model_rects.empty()) {
            history.record_rects(std::move(stroke.model_rects));
        }
        stroke.end();
    }
    if (event->button() == Qt::MiddleButton) {
        panning = false;
        setCursor(selected_tool != WorldTool::Inspect ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void WorldMapCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const QPointF cursor = event->position();
    const QRectF old_bounds = mapBounds();
    const Json& map = project->maps.at(current_map);
    const qreal old_cell = old_bounds.width() / map["w"].as_int();
    const QPointF map_position((cursor.x() - old_bounds.left()) / old_cell,
                               (cursor.y() - old_bounds.top()) / old_cell);
    qreal updated_zoom = zoom_factor;
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        constexpr std::array<qreal, 5> snapSteps{1.0, 2.0, 4.0, 8.0, 16.0};
        int snapIndex = 0;
        for (int index = 1; index < static_cast<int>(snapSteps.size()); ++index) {
            if (std::abs(std::log(snapSteps[static_cast<std::size_t>(index)] / zoom_factor)) <
                std::abs(std::log(snapSteps[static_cast<std::size_t>(snapIndex)] / zoom_factor))) {
                snapIndex = index;
            }
        }
        if (event->angleDelta().y() > 0) {
            while (snapIndex + 1 < static_cast<int>(snapSteps.size()) &&
                   snapSteps[static_cast<std::size_t>(snapIndex)] <= zoom_factor + 0.001) {
                ++snapIndex;
            }
        } else {
            while (snapIndex > 0 && snapSteps[static_cast<std::size_t>(snapIndex)] >=
                   zoom_factor - 0.001) {
                --snapIndex;
            }
        }
        updated_zoom = snapSteps[static_cast<std::size_t>(snapIndex)];
    } else {
        const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        updated_zoom = std::clamp(zoom_factor * factor, 0.05, 16.0);
    }
    if (updated_zoom == zoom_factor) return;
    zoom_factor = updated_zoom;
    const QRectF new_bounds = mapBounds();
    const qreal new_cell = new_bounds.width() / map["w"].as_int();
    pan_offset += cursor - QPointF(new_bounds.left() + map_position.x() * new_cell,
                                   new_bounds.top() + map_position.y() * new_cell);
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
    event->accept();
}

void WorldMapCanvasWidget::leaveEvent(QEvent* event) {
    hover_tile = {-1, -1};
    refreshCanvas();
    QWidget::leaveEvent(event);
}

WorldPanel::WorldPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("WORLD MAP", this);
    title->setStyleSheet("font-size: 15px; font-weight: 700; color: #e8edf2; letter-spacing: 1px;");
    header->addWidget(title);
    header->addSpacing(14);
    header->addWidget(new QLabel("Map:", this));
    mapCombo = new QComboBox(this);
    mapCombo->setMinimumWidth(220);
    header->addWidget(mapCombo);
    header->addSpacing(12);
    header->addWidget(new QLabel("Tool:", this));
    toolCombo = new QComboBox(this);
    for (const std::string_view name : world::kWorldToolNames) {
        toolCombo->addItem(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    }
    header->addWidget(toolCombo);
    auto* undoButton = new QPushButton("Undo", this);
    auto* redoButton = new QPushButton("Redo", this);
    header->addWidget(undoButton);
    header->addWidget(redoButton);
    header->addSpacing(10);
    header->addWidget(new QLabel("Zoom:", this));
    zoomSpin = new QSpinBox(this);
    zoomSpin->setRange(5, 1600);
    zoomSpin->setSingleStep(25);
    zoomSpin->setValue(100);
    zoomSpin->setSuffix("%");
    header->addWidget(zoomSpin);
    auto* fitButton = new QPushButton("Fit", this);
    header->addWidget(fitButton);
    auto* overlayButton = new QToolButton(this);
    overlayButton->setText("Climate layers");
    overlayButton->setPopupMode(QToolButton::InstantPopup);
    auto* overlayMenu = new QMenu(overlayButton);
    QAction* temperatureAction = overlayMenu->addAction("Temperature / equator / poles");
    temperatureAction->setCheckable(true);
    QAction* windAction = overlayMenu->addAction("Wind");
    windAction->setCheckable(true);
    QAction* currentsAction = overlayMenu->addAction("Ocean currents");
    currentsAction->setCheckable(true);
    QAction* weatherAction = overlayMenu->addAction("Weather");
    weatherAction->setCheckable(true);
    QAction* dayNightAction = overlayMenu->addAction("Day/night lighting");
    dayNightAction->setCheckable(true);
    dayNightAction->setChecked(true);
    QAction* contoursAction = overlayMenu->addAction("Terrain contours and pixel grid");
    contoursAction->setCheckable(true);
    contoursAction->setChecked(true);
    overlayMenu->addSeparator();
    QAction* simulationAction = overlayMenu->addAction("Run simulation");
    simulationAction->setCheckable(true);
    overlayButton->setMenu(overlayMenu);
    header->addWidget(new QLabel("Weather:", this));
    weatherCombo = new QComboBox(this);
    weatherCombo->addItem("Climate", "climate");
    weatherCombo->addItem("Clear", "clear");
    weatherCombo->addItem("Cloudy", "cloudy");
    weatherCombo->addItem("Rain", "rain");
    weatherCombo->addItem("Snow", "snow");
    weatherCombo->addItem("Storm", "storm");
    weatherCombo->addItem("Fog", "fog");
    weatherCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    weatherCombo->setMinimumContentsLength(7);
    weatherCombo->setToolTip(
        "Preview a climate-driven weather mix or force a weather condition on the world map.");
    timeOfDayEdit = new QTimeEdit(QTime(8, 0), this);
    timeOfDayEdit->setDisplayFormat("HH:mm");
    timeOfDayEdit->setToolTip("Preview world lighting and terrain shading at this local time.");
    header->addWidget(overlayButton);
    header->addWidget(weatherCombo);
    header->addWidget(new QLabel("Time:", this));
    header->addWidget(timeOfDayEdit);
    header->addStretch(1);
    generateWorldButton = new QPushButton(QStringLiteral("Generate new world\u2026"), this);
    header->addWidget(generateWorldButton);
    mapInfo = new QLabel(this);
    mapInfo->setStyleSheet("color: #aeb8c2;");
    header->addWidget(mapInfo);
    layout->addLayout(header);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* sidebar = new QWidget(splitter);
    auto* sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(0, 0, 8, 0);
    sidebar_layout->setSpacing(6);

    auto* biome_title = new QLabel("BIOMES", sidebar);
    biome_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(biome_title);
    biomeList = new QListWidget(sidebar);
    biomeList->setMaximumHeight(190);
    sidebar_layout->addWidget(biomeList);
    newBiomeButton = new QPushButton(QStringLiteral("Create biome\u2026"), sidebar);
    sidebar_layout->addWidget(newBiomeButton);

    auto* terrain_title = new QLabel("TERRAIN", sidebar);
    terrain_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(terrain_title);
    auto* landformRow = new QHBoxLayout();
    landformCombo = new QComboBox(sidebar);
    landformCombo->addItems({"Selected tile", "River", "Lake", "Beach", "Mountain range",
                             "Hills", "Cave"});
    brushSizeSpin = new QSpinBox(sidebar);
    brushSizeSpin->setRange(1, 15);
    brushSizeSpin->setSingleStep(2);
    brushSizeSpin->setValue(1);
    brushSizeSpin->setToolTip("Odd brush width in map tiles; use a larger brush for lakes and ranges.");
    landformRow->addWidget(landformCombo, 1);
    landformRow->addWidget(brushSizeSpin);
    sidebar_layout->addLayout(landformRow);
    auto* tileToolsRow = new QHBoxLayout();
    tileCategoryCombo = new QComboBox(sidebar);
    tileCategoryCombo->addItem("All terrain", "all");
    tileCategoryCombo->addItem("Biome starters", "biomes");
    tileCategoryCombo->addItem("Surface", "surface");
    tileCategoryCombo->addItem("Coasts", "coast");
    tileCategoryCombo->addItem("Underwater", "underwater");
    tileCategoryCombo->addItem("Elevation", "elevation");
    tileCategoryCombo->addItem("Structures", "structures");
    tileCategoryCombo->setToolTip("Filter the terrain tile palette by environment.");
    auto* addStarterTilesButton = new QPushButton("Add starter set", sidebar);
    addStarterTilesButton->setToolTip(
        "Add 9 general terrain tiles and original terrain for all 12 project biomes.");
    tileToolsRow->addWidget(tileCategoryCombo, 1);
    tileToolsRow->addWidget(addStarterTilesButton);
    sidebar_layout->addLayout(tileToolsRow);
    tilePalette = new QListWidget(sidebar);
    tilePalette->setSelectionMode(QAbstractItemView::SingleSelection);
    tilePalette->setMaximumHeight(190);
    sidebar_layout->addWidget(tilePalette);
    auto* tileHelp = new QLabel(
        "Select a tile, then Paint terrain and click/drag. HD maps layer tiles over relief.", sidebar);
    tileHelp->setWordWrap(true);
    tileHelp->setStyleSheet("color: #8d98a3;");
    sidebar_layout->addWidget(tileHelp);

    auto* nodes_title = new QLabel("LOCATIONS & EXITS", sidebar);
    nodes_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(nodes_title);
    nodeList = new QListWidget(sidebar);
    sidebar_layout->addWidget(nodeList, 1);
    auto* node_buttons = new QHBoxLayout();
    addLocationButton = new QPushButton("Add Location", sidebar);
    addExitButton = new QPushButton("Add Exit", sidebar);
    node_buttons->addWidget(addLocationButton);
    node_buttons->addWidget(addExitButton);
    sidebar_layout->addLayout(node_buttons);
    auto* node_actions = new QHBoxLayout();
    editNodeButton = new QPushButton("Edit", sidebar);
    deleteNodeButton = new QPushButton("Delete", sidebar);
    editNodeButton->setEnabled(false);
    deleteNodeButton->setEnabled(false);
    node_actions->addWidget(editNodeButton);
    node_actions->addWidget(deleteNodeButton);
    sidebar_layout->addLayout(node_actions);
    splitter->addWidget(sidebar);

    mapCanvas = new WorldMapCanvasWidget(splitter);
    mapCanvas->setProject(project);
    splitter->addWidget(mapCanvas);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({235, 760});
    layout->addWidget(splitter, 1);
    generation_job = new world::WorldGenerationJob(this, this);
    connect(generation_job, &world::WorldGenerationJob::generationFinished, this,
            [this](const QString& map_id, const Json& map) {
        const std::string id = map_id.toStdString();
        if (project) project->maps[id] = map;
        current_map = id;
        refreshGraph();
    });
    connect(generation_job, &world::WorldGenerationJob::generationFailed, this,
            [this](const QString& message) {
        QMessageBox::warning(this, "World generation failed", message);
    });
    connect(undoButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::undo);
    connect(redoButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::redo);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, mapCanvas, &WorldMapCanvasWidget::undo);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, mapCanvas, &WorldMapCanvasWidget::redo);

    auto* footer = new QLabel(QStringLiteral("Wheel: zoom \u00b7 Ctrl+wheel: snap 1/2/4/8/16\u00d7 \u00b7 middle-drag: pan \u00b7 "
                              "Keys 1-7: tool \u00b7 Ctrl+Z/Y: undo/redo \u00b7 "
                              "Raise/Lower height or paint biome \u00b7 Add Location/Exit, then click \u00b7 "
                              "Fit: show the whole world \u00b7 Save with Ctrl+S."), this);
    footer->setStyleSheet("color: #8d98a3;");
    layout->addWidget(footer);

    connect(mapCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) selectMap(mapCombo->itemData(index).toString().toStdString());
    });
    connect(toolCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                const int bounded = std::clamp(index, 0, world::kWorldToolCount - 1);
                mapCanvas->setTool(static_cast<WorldTool>(bounded));
            });
    for (int key = 1; key <= 7; ++key) {
        QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_0 + key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key]() {
            toolCombo->setCurrentIndex(key - 1);
        });
    }
    toolCombo->setToolTip("Tool (keys 1-7: Inspect, Paint terrain, Paint biome, "
                          "Place location, Place exit, Raise height, Lower height)");
    connect(brushSizeSpin, &QSpinBox::valueChanged, mapCanvas,
            &WorldMapCanvasWidget::setBrushSize);
    connect(tileCategoryCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { filterTerrainTiles(); });
    connect(addStarterTilesButton, &QPushButton::clicked, this,
            [this]() { addStarterTerrainTiles(); });
    connect(landformCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        static const std::array<int, 7> tiles = {-1, 7, 7, 6, 4, 5, 72};
        static const std::array<int, 7> sizes = {1, 1, 5, 3, 5, 3, 1};
        if (index <= 0 || index >= static_cast<int>(tiles.size())) return;
        if (!project || !project->maps.contains(current_map)) return;
        const auto tileset_it = project->tilesets.find(
            project->maps[current_map]["tileset"].as_string());
        if (tileset_it == project->tilesets.end() ||
            tiles[index] >= static_cast<int>(tileset_it->second["tiles"].as_array().size())) {
            QMessageBox::warning(this, "Landform brush",
                                 "This world tileset is missing the required landform tile.");
            landformCombo->setCurrentIndex(0);
            return;
        }
        mapCanvas->setSelectedTile(tiles[index]);
        brushSizeSpin->setValue(sizes[index]);
        toolCombo->setCurrentIndex(1);
        for (int row = 0; row < tilePalette->count(); ++row) {
            if (tilePalette->item(row)->data(Qt::UserRole).toInt() == tiles[index]) {
                tilePalette->setCurrentRow(row);
                break;
            }
        }
    });
    connect(zoomSpin, &QSpinBox::valueChanged, this, [this](int value) {
        mapCanvas->setZoom(value / 100.0);
    });
    connect(fitButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::fitToView);
    connect(temperatureAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Temperature, enabled); });
    connect(windAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Wind, enabled); });
    connect(currentsAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Currents, enabled); });
    connect(weatherAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::Weather, enabled); });
    connect(weatherCombo, &QComboBox::currentIndexChanged, this,
            [this, weatherAction](int index) {
                const std::string weather =
                    weatherCombo->itemData(index).toString().toStdString();
                mapCanvas->setWeatherType(weather);
                if (weather != "climate" && !weatherAction->isChecked()) {
                    weatherAction->setChecked(true);
                }
            });
    connect(dayNightAction, &QAction::toggled, mapCanvas,
            [this](bool enabled) { mapCanvas->setOverlay(WorldOverlay::DayNight, enabled); });
    connect(contoursAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setContoursEnabled);
    connect(simulationAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setSimulationRunning);
    connect(timeOfDayEdit, &QTimeEdit::timeChanged, this, [this](const QTime& time) {
        mapCanvas->setTimeOfDay(time.hour(), time.minute());
    });
    connect(mapCanvas, &WorldMapCanvasWidget::timeOfDayChanged, this,
            [this](int hour, int minute) {
                const QSignalBlocker blocker(timeOfDayEdit);
                timeOfDayEdit->setTime(QTime(hour, minute));
            });
    connect(mapCanvas, &WorldMapCanvasWidget::zoomChanged, this, [this](qreal zoom) {
        const QSignalBlocker blocker(zoomSpin);
        zoomSpin->setValue(qRound(zoom * 100.0));
    });
    connect(tilePalette, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const int tile = tilePalette->item(row)->data(Qt::UserRole).toInt();
        mapCanvas->setSelectedTile(tile);
        if (landformCombo->currentIndex() != 0) landformCombo->setCurrentIndex(0);
    });
    connect(biomeList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        selected_biome = biomeList->item(row)->data(Qt::UserRole).toString().toStdString();
        mapCanvas->setSelectedBiome(selected_biome);
    });
    connect(nodeList, &QListWidget::currentRowChanged, this, [this](int row) {
        const QListWidgetItem* item = row >= 0 ? nodeList->item(row) : nullptr;
        const int entity_index = item ? item->data(Qt::UserRole).toInt() : -1;
        mapCanvas->setSelectedNode(entity_index);
        editNodeButton->setEnabled(entity_index >= 0);
        deleteNodeButton->setEnabled(entity_index >= 0);
    });
    connect(nodeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { editWorldEntity(); });
    connect(mapCanvas, &WorldMapCanvasWidget::entitySelected, this, [this](int entity_index) {
        int row = -1;
        for (int index = 0; index < nodeList->count(); ++index) {
            if (nodeList->item(index)->data(Qt::UserRole).toInt() == entity_index) {
                row = index;
                break;
            }
        }
        nodeList->setCurrentRow(row);
    });
    connect(mapCanvas, &WorldMapCanvasWidget::entitiesChanged, this,
            [this]() { refreshMapContents(); });
    connect(mapCanvas, &WorldMapCanvasWidget::entityPlacementRequested, this,
            [this](const QPoint& tile, WorldTool tool) { createWorldEntity(tile, tool); });
    connect(addLocationButton, &QPushButton::clicked, this, [this]() {
        toolCombo->setCurrentIndex(3);
    });
    connect(addExitButton, &QPushButton::clicked, this, [this]() {
        toolCombo->setCurrentIndex(4);
    });
    connect(editNodeButton, &QPushButton::clicked, this, [this]() { editWorldEntity(); });
    connect(deleteNodeButton, &QPushButton::clicked, this, [this]() { deleteWorldEntity(); });
    connect(newBiomeButton, &QPushButton::clicked, this, [this]() { createBiome(); });
    connect(generateWorldButton, &QPushButton::clicked, this, [this]() {
        if (!project || !project->tilesets.contains("world") ||
            !project->tilesets["world"].contains("tiles") ||
            project->tilesets["world"]["tiles"].as_array().size() < 8) {
            QMessageBox::warning(this, "Generate world", "World generation needs a world tileset with at least 8 terrain tiles.");
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle("Generate a new world");
        QFormLayout form(&dialog);
        auto* namingHint = new QLabel(
            "Create an original spherical world with editable terrain and a shaded, high-detail surface. "
            "The seed reproduces the same landforms; repeated seeds get a numbered copy.",
            &dialog);
        namingHint->setWordWrap(true);
        form.addRow(namingHint);
        auto* topology = new QComboBox(&dialog);
        topology->addItem("Natural continents", "continents");
        topology->addItem("Island chains", "archipelago");
        topology->addItem("One vast supercontinent", "supercontinent");
        topology->addItem("Fractured / mountainous", "fractured");
        topology->setToolTip("Choose the broad landform style. A new seed changes the actual geography.");
        form.addRow("World topology:", topology);
        auto* width = new QSpinBox(&dialog);
        width->setRange(24, 2560);
        width->setValue(256);
        auto* height = new QSpinBox(&dialog);
        height->setRange(16, 1280);
        height->setValue(128);
        form.addRow("Width (tiles):", width);
        form.addRow("Height (tiles):", height);
        auto* resolutionHint = new QLabel(
            "Default is 256 \u00d7 128 tiles (the Sylvi diorama scale). Larger maps remain available "
            "up to 2560 \u00d7 1280, but can use several GB of editor memory.",
            &dialog);
        resolutionHint->setWordWrap(true);
        form.addRow(resolutionHint);
        auto* seed = new QSpinBox(&dialog);
        seed->setRange(-1000000000, 1000000000);
        seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
        auto* randomSeedButton = new QPushButton("Randomize", &dialog);
        auto* seedRow = new QHBoxLayout();
        seedRow->addWidget(seed);
        seedRow->addWidget(randomSeedButton);
        form.addRow("Seed:", seedRow);
        connect(randomSeedButton, &QPushButton::clicked, &dialog, [seed]() {
            seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
        });
        auto* water = new QSpinBox(&dialog);
        water->setRange(0, 100);
        water->setValue(50);
        water->setSuffix("%");
        form.addRow("Water level:", water);
        auto* vegetation = new QSpinBox(&dialog);
        vegetation->setRange(0, 100);
        vegetation->setValue(65);
        vegetation->setSuffix("%");
        form.addRow("Vegetation:", vegetation);
        auto* decay = new QSpinBox(&dialog);
        decay->setRange(0, 100);
        decay->setValue(15);
        decay->setSuffix("%");
        form.addRow("Decay / ruins:", decay);
        auto* axialTilt = new QSpinBox(&dialog);
        axialTilt->setRange(0, 359);
        axialTilt->setValue(0);
        axialTilt->setSuffix(QStringLiteral("\u00b0"));
        axialTilt->setToolTip(QStringLiteral("0\u00b0 places the equator horizontally; this rotates the equator and pole axis on the map."));
        form.addRow("Axial tilt / equator angle:", axialTilt);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;

        const int seed_value = seed->value();
        const std::string seed_text = std::to_string(seed_value);
        const std::string base_id = world::make_biome_id("new_world_seed_" + seed_text);
        const std::string base_name = "New World " + seed_text;
        std::string id = base_id;
        std::string name = base_name;
        int copy_number = 2;
        while (project->maps.contains(id)) {
            id = base_id + "_" + std::to_string(copy_number);
            name = base_name + " (" + std::to_string(copy_number) + ")";
            ++copy_number;
        }
        world::WorldGenerationParams params;
        params.map_id = id;
        params.map_name = name;
        params.width = width->value();
        params.height = height->value();
        params.seed = seed_value;
        params.topology = topology->currentData().toString().toStdString();
        params.water = water->value();
        params.vegetation = vegetation->value();
        params.decay = decay->value();
        params.axial_tilt = axialTilt->value();
        generation_job->start(std::move(params), project);
    });

    refreshGraph();
}

void WorldPanel::setProject(Project* p) {
    project = p;
    mapCanvas->setProject(p);
    refreshGraph();
}

void WorldPanel::refreshGraph() {
    if (!mapCombo) return;
    mapCombo->blockSignals(true);
    mapCombo->clear();

    if (project) {
        for (const auto& [map_id, map] : project->maps) {
            if (map.contains("kind") && map["kind"].as_string() == "world") {
                const std::string name = map.contains("name") ? map["name"].as_string() : map_id;
                mapCombo->addItem(QString::fromStdString(name), QString::fromStdString(map_id));
            }
        }
    }

    int selected_index = -1;
    for (int index = 0; index < mapCombo->count(); ++index) {
        if (mapCombo->itemData(index).toString().toStdString() == current_map) {
            selected_index = index;
            break;
        }
    }
    if (selected_index < 0 && mapCombo->count() > 0) selected_index = 0;
    mapCombo->setCurrentIndex(selected_index);
    mapCombo->blockSignals(false);

    if (selected_index >= 0) selectMap(mapCombo->itemData(selected_index).toString().toStdString());
    else {
        current_map.clear();
        mapCanvas->setMap({});
        tilePalette->clear();
        biomeList->clear();
        nodeList->clear();
        mapInfo->setText("No world maps in project");
    }
}

void WorldPanel::selectMap(const std::string& map_id) {
    current_map = map_id;
    mapCanvas->setMap(map_id);
    refreshMapContents();
}

void WorldPanel::refreshMapContents() {
    tilePalette->clear();
    biomeList->clear();
    nodeList->clear();
    mapCanvas->setSelectedNode(-1);
    editNodeButton->setEnabled(false);
    deleteNodeButton->setEnabled(false);
    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        mapInfo->clear();
        return;
    }

    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map["tileset"].as_string();
    QString info = QString(QStringLiteral("%1 \u00d7 %2 \u00b7 %3"))
        .arg(map["w"].as_int()).arg(map["h"].as_int())
        .arg(QString::fromStdString(tileset_id));
    if (map.contains("worldgen") && map["worldgen"].contains("style") &&
        map["worldgen"]["style"].as_string() == "hd2d") {
        info += QString(QStringLiteral(" \u00b7 HD \u00b7 %1")).arg(QString::fromStdString(
            map["worldgen"]["topology"].as_string()));
    }
    mapInfo->setText(info);

    const auto tileset = project->tilesets.find(tileset_id);
    if (tileset != project->tilesets.end()) {
        const auto& tiles = tileset->second["tiles"].as_array();
        tilePalette->setIconSize(QSize(40, 40));
        for (size_t index = 0; index < tiles.size(); ++index) {
            const Json& tile = tiles[index];
            const std::string name = tile.contains("name")
                ? tile["name"].as_string() : "Tile " + std::to_string(index);
            auto* item = new QListWidgetItem(
                QString("%1  %2").arg(index, 2, 10, QChar('0')).arg(QString::fromStdString(name)),
                tilePalette);
            const QImage preview = make_tileset_tile(*project, tileset_id, static_cast<int>(index));
            if (!preview.isNull()) {
                item->setIcon(QIcon(QPixmap::fromImage(preview).scaled(
                    40, 40, Qt::KeepAspectRatio, Qt::FastTransformation)));
            }
            item->setData(Qt::UserRole, static_cast<int>(index));
            item->setData(Qt::UserRole + 1,
                          QString::fromStdString(world::terrain_tile_category(name)));
        }
    }
    if (tilePalette->count() > 0) tilePalette->setCurrentRow(0);
    filterTerrainTiles();

    for (const auto& [biome_id, biome] : project->biomes) {
        if (!biome.contains("tileset") || biome["tileset"].as_string() != tileset_id) continue;
        const std::string name = biome.contains("name") ? biome["name"].as_string() : biome_id;
        auto* item = new QListWidgetItem(QString::fromStdString(name), biomeList);
        item->setData(Qt::UserRole, QString::fromStdString(biome_id));
        if (biome.contains("color")) {
            QColor color(QString::fromStdString(biome["color"].as_string()));
            if (color.isValid()) item->setForeground(color);
        }
        if (biome_id == selected_biome) biomeList->setCurrentItem(item);
    }
    if (biomeList->count() > 0 && biomeList->currentRow() < 0) biomeList->setCurrentRow(0);
    if (biomeList->currentItem()) {
        selected_biome = biomeList->currentItem()->data(Qt::UserRole).toString().toStdString();
        mapCanvas->setSelectedBiome(selected_biome);
    }

    if (map.contains("entities") && map["entities"].is_array()) {
        const auto& entities = map["entities"].as_array();
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& entity = entities[index];
            const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
            if (type != "location" && type != "exit") continue;
            std::string label = type == "exit" ? "Exit" : "Location";
            if (entity.contains("target") && entity["target"].contains("map")) {
                label += " \u2192 " + entity["target"]["map"].as_string();
                if (entity["target"].contains("spawn") && entity["target"]["spawn"].is_string() &&
                    !entity["target"]["spawn"].as_string().empty()) {
                    label += " / " + entity["target"]["spawn"].as_string();
                }
            } else if (entity.contains("id")) {
                label += " \u00b7 " + entity["id"].as_string();
            }
            label += "  (" + std::to_string(entity["x"].as_int()) + ", " +
                     std::to_string(entity["y"].as_int()) + ")";
            auto* item = new QListWidgetItem(QString::fromStdString(label), nodeList);
            item->setData(Qt::UserRole, static_cast<int>(index));
        }
    }
}

void WorldPanel::filterTerrainTiles() {
    if (!tilePalette || !tileCategoryCombo) return;
    const QString category = tileCategoryCombo->currentData().toString();
    int first_visible = -1;
    for (int row = 0; row < tilePalette->count(); ++row) {
        QListWidgetItem* item = tilePalette->item(row);
        const bool visible = category == "all" ||
            item->data(Qt::UserRole + 1).toString() == category;
        item->setHidden(!visible);
        if (visible && first_visible < 0) first_visible = row;
    }
    const int selected_row = tilePalette->currentRow();
    if (selected_row < 0 || tilePalette->item(selected_row)->isHidden()) {
        tilePalette->setCurrentRow(first_visible);
    }
}

void WorldPanel::addStarterTerrainTiles() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const std::string tileset_id = project->maps.at(current_map)["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    if (tileset_it == project->tilesets.end() ||
        !tileset_it->second.contains("tiles") ||
        !tileset_it->second["tiles"].is_array()) {
        QMessageBox::warning(this, "Add starter terrain tiles",
                             "The selected world map has no editable tileset.");
        return;
    }

    JsonArray& tiles = tileset_it->second["tiles"].as_array();
    const Json* palettes = tileset_it->second.contains("palettes") &&
        tileset_it->second["palettes"].is_array()
        ? &tileset_it->second["palettes"] : nullptr;
    const int palette_count = palettes ? static_cast<int>(palettes->as_array().size()) : 0;
    int added = 0;
    for (const Json& starter : generate_starter_world_terrain_tiles(palette_count)) {
        const bool exists = std::any_of(tiles.begin(), tiles.end(), [&starter](const Json& tile) {
            return tile.is_object() && tile.contains("name") &&
                tile["name"].as_string() == starter["name"].as_string();
        });
        if (exists) continue;

        tiles.push_back(starter);
        ++added;
    }

    if (added == 0) {
        QMessageBox::information(this, "Add starter terrain tiles",
                                 "This tileset already has the complete starter terrain set.");
        return;
    }
    refreshMapContents();
}

void WorldPanel::createBiome() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    if (tileset_it == project->tilesets.end() ||
        !tileset_it->second.contains("tiles") || tileset_it->second["tiles"].as_array().empty()) {
        QMessageBox::warning(this, "Create biome", "The selected map has no usable tileset.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Create biome");
    QFormLayout form(&dialog);
    QLineEdit nameEdit(&dialog);
    nameEdit.setPlaceholderText("e.g. Moonlit Coast");
    form.addRow("Name:", &nameEdit);

    QComboBox climateCombo(&dialog);
    climateCombo.addItems({"temperate", "tropical", "arid", "arctic", "humid", "volcanic", "arcane"});
    form.addRow("Climate:", &climateCombo);

    QComboBox baseTileCombo(&dialog);
    const auto& tiles = tileset_it->second["tiles"].as_array();
    for (size_t index = 0; index < tiles.size(); ++index) {
        const std::string name = tiles[index].contains("name")
            ? tiles[index]["name"].as_string() : "Tile " + std::to_string(index);
        baseTileCombo.addItem(QString(QStringLiteral("%1 \u00b7 %2")).arg(index).arg(QString::fromStdString(name)),
                              static_cast<int>(index));
    }
    if (tilePalette->currentRow() >= 0) baseTileCombo.setCurrentIndex(tilePalette->currentRow());
    form.addRow("Base terrain:", &baseTileCombo);

    auto* colorEdit = new QLineEdit("#75b96b", &dialog);
    auto* colorButton = new QPushButton(QStringLiteral("Choose\u2026"), &dialog);
    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(colorEdit);
    colorRow->addWidget(colorButton);
    form.addRow("Map tint:", colorRow);
    connect(colorButton, &QPushButton::clicked, &dialog, [&dialog, colorEdit]() {
        const QColor selected = QColorDialog::getColor(QColor(colorEdit->text()), &dialog,
                                                       "Biome overlay color");
        if (selected.isValid()) colorEdit->setText(selected.name());
    });

    QLineEdit descriptionEdit(&dialog);
    descriptionEdit.setPlaceholderText("Optional notes about this region");
    form.addRow("Description:", &descriptionEdit);
    auto add_percent_field = [&form, &dialog](const QString& label, int value) {
        auto* field = new QSpinBox(&dialog);
        field->setRange(0, 100);
        field->setValue(value);
        field->setSuffix("%");
        form.addRow(label, field);
        return field;
    };
    QSpinBox* vegetation = add_percent_field("Vegetation:", 60);
    QSpinBox* decay = add_percent_field("Decay:", 10);
    QSpinBox* waterLevel = add_percent_field("Water level:", 50);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    const std::string name = nameEdit.text().trimmed().toStdString();
    const QColor color(colorEdit->text());
    if (name.empty() || !color.isValid()) {
        QMessageBox::warning(this, "Create biome", "Enter a biome name and a valid color.");
        return;
    }

    std::string id = world::make_biome_id(name);
    if (id.empty()) {
        QMessageBox::warning(this, "Create biome", "The name must contain at least one letter or number.");
        return;
    }
    const std::string base_id = id;
    int suffix = 2;
    while (project->biomes.contains(id)) id = base_id + "_" + std::to_string(suffix++);

    Json biome;
    biome["name"] = name;
    biome["climate"] = climateCombo.currentText().toStdString();
    biome["tileset"] = tileset_id;
    biome["base_tile"] = baseTileCombo.currentData().toInt();
    biome["color"] = color.name().toStdString();
    biome["description"] = descriptionEdit.text().trimmed().toStdString();
    biome["vegetation"] = vegetation->value();
    biome["decay"] = decay->value();
    biome["water_level"] = waterLevel->value();
    project->biomes[id] = biome;
    selected_biome = id;
    refreshMapContents();
}

void WorldPanel::createWorldEntity(const QPoint& tile, WorldTool tool) {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !world::is_entity_placement(tool)) return;
    Json& map = project->maps[current_map];
    if (tile.x() < 0 || tile.y() < 0 || tile.x() >= map["w"].as_int() ||
        tile.y() >= map["h"].as_int()) return;
    const JsonArray no_entities;
    const JsonArray& existing_entities = map.contains("entities") && map["entities"].is_array()
        ? map["entities"].as_array() : no_entities;
    int added_index = -1;

    if (tool == WorldTool::PlaceLocation) {
        bool accepted = false;
        const QString entered_id = QInputDialog::getText(
            this, "Add world location", "Location ID:", QLineEdit::Normal,
            "new_location", &accepted).trimmed();
        if (!accepted) return;
        const std::string id = world::make_biome_id(entered_id.toStdString());
        if (id.empty()) {
            QMessageBox::warning(this, "Add world location",
                                 "Enter a location ID containing at least one letter or number.");
            return;
        }
        for (const Json& entity : existing_entities) {
            if (entity.contains("id") && entity["id"].is_string() &&
                entity["id"].as_string() == id) {
                QMessageBox::warning(this, "Add world location",
                                     "That location ID is already in use on this map.");
                return;
            }
        }
        mapCanvas->recordEntityEdit();
        JsonArray& entities = map["entities"].as_array();
        entities.emplace_back(Json(JsonObject{
            {"id", id}, {"type", "location"}, {"x", tile.x()}, {"y", tile.y()}
        }));
        added_index = static_cast<int>(entities.size()) - 1;
    } else {
        QDialog dialog(this);
        dialog.setWindowTitle("Add world exit");
        QFormLayout form(&dialog);
        QComboBox targetMap(&dialog);
        for (const auto& [map_id, target] : project->maps) {
            const std::string name = target.contains("name") && target["name"].is_string()
                ? target["name"].as_string() : map_id;
            targetMap.addItem(QString::fromStdString(name), QString::fromStdString(map_id));
        }
        if (targetMap.count() == 0) {
            QMessageBox::warning(this, "Add world exit", "The project has no target maps.");
            return;
        }
        form.addRow("Destination map:", &targetMap);
        QLineEdit targetSpawn(&dialog);
        targetSpawn.setPlaceholderText("Optional spawn ID");
        form.addRow("Destination spawn:", &targetSpawn);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;

        JsonObject target{{"map", targetMap.currentData().toString().toStdString()}};
        const std::string spawn = targetSpawn.text().trimmed().toStdString();
        if (!spawn.empty()) target["spawn"] = spawn;
        mapCanvas->recordEntityEdit();
        JsonArray& entities = map["entities"].as_array();
        entities.emplace_back(Json(JsonObject{
            {"effect", "fade"}, {"h", 1}, {"target", Json(target)},
            {"type", "exit"}, {"w", 1}, {"x", tile.x()}, {"y", tile.y()}
        }));
        added_index = static_cast<int>(entities.size()) - 1;
    }

    refreshMapContents();
    for (int row = 0; row < nodeList->count(); ++row) {
        if (nodeList->item(row)->data(Qt::UserRole).toInt() == added_index) {
            nodeList->setCurrentRow(row);
            break;
        }
    }
}

void WorldPanel::editWorldEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !nodeList->currentItem()) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    JsonArray& entities = map["entities"].as_array();
    const int entity_index = nodeList->currentItem()->data(Qt::UserRole).toInt();
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    Json& entity = entities[static_cast<size_t>(entity_index)];
    const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
    if (type != "location" && type != "exit") return;

    QDialog dialog(this);
    dialog.setWindowTitle(type == "exit" ? "Edit world exit" : "Edit world location");
    QFormLayout form(&dialog);
    QLineEdit idEdit(&dialog);
    if (entity.contains("id") && entity["id"].is_string()) {
        idEdit.setText(QString::fromStdString(entity["id"].as_string()));
    }
    QSpinBox x(&dialog);
    QSpinBox y(&dialog);
    x.setRange(0, map["w"].as_int() - 1);
    y.setRange(0, map["h"].as_int() - 1);
    x.setValue(entity["x"].as_int());
    y.setValue(entity["y"].as_int());
    if (type == "location") {
        form.addRow("Location ID:", &idEdit);
    }
    form.addRow("X (tiles):", &x);
    form.addRow("Y (tiles):", &y);

    QComboBox targetMap(&dialog);
    QLineEdit targetSpawn(&dialog);
    if (type == "exit") {
        for (const auto& [map_id, target] : project->maps) {
            const std::string name = target.contains("name") && target["name"].is_string()
                ? target["name"].as_string() : map_id;
            targetMap.addItem(QString::fromStdString(name), QString::fromStdString(map_id));
        }
        if (entity.contains("target") && entity["target"].contains("map")) {
            const int target_index = targetMap.findData(
                QString::fromStdString(entity["target"]["map"].as_string()));
            if (target_index >= 0) targetMap.setCurrentIndex(target_index);
        }
        if (entity.contains("target") && entity["target"].contains("spawn") &&
            entity["target"]["spawn"].is_string()) {
            targetSpawn.setText(QString::fromStdString(entity["target"]["spawn"].as_string()));
        }
        targetSpawn.setPlaceholderText("Optional spawn ID");
        form.addRow("Destination map:", &targetMap);
        form.addRow("Destination spawn:", &targetSpawn);
    }
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    std::string id;
    if (type == "location") {
        id = world::make_biome_id(idEdit.text().trimmed().toStdString());
        if (id.empty()) {
            QMessageBox::warning(this, "Edit world location",
                                 "Enter a location ID containing at least one letter or number.");
            return;
        }
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& other = entities[index];
            if (static_cast<int>(index) != entity_index && other.contains("id") &&
                other["id"].is_string() && other["id"].as_string() == id) {
                QMessageBox::warning(this, "Edit world location",
                                     "That location ID is already in use on this map.");
                return;
            }
        }
    } else if (targetMap.count() == 0) {
        QMessageBox::warning(this, "Edit world exit", "The project has no target maps.");
        return;
    }

    mapCanvas->recordEntityEdit();
    if (type == "location") {
        entity["id"] = id;
    } else {
        entity["target"]["map"] = targetMap.currentData().toString().toStdString();
        entity["target"]["spawn"] = targetSpawn.text().trimmed().toStdString();
    }
    entity["x"] = x.value();
    entity["y"] = y.value();
    refreshMapContents();
    for (int row = 0; row < nodeList->count(); ++row) {
        if (nodeList->item(row)->data(Qt::UserRole).toInt() == entity_index) {
            nodeList->setCurrentRow(row);
            break;
        }
    }
}

void WorldPanel::deleteWorldEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !nodeList->currentItem()) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    JsonArray& entities = map["entities"].as_array();
    const int entity_index = nodeList->currentItem()->data(Qt::UserRole).toInt();
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    const auto answer = QMessageBox::question(
        this, "Delete world marker", "Delete the selected location or exit?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    mapCanvas->recordEntityEdit();
    entities.erase(entities.begin() + entity_index);
    refreshMapContents();
}

} // namespace aur






