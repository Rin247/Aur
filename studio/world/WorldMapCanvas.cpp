#include "world/WorldMapCanvas.hpp"
#include "RhiBackend.hpp"
#include "TileTexture.hpp"
#include "world/HdSurface.hpp"
#include "world/TerrainLod.hpp"
#include "world/WorldTerrainPaint.hpp"
#include "world/WorldMapHelpers.hpp"
#include "world/MapView.hpp"
#include "world/WorldWeatherOverlay.hpp"
#include "world/WorldClimateOverlays.hpp"
#include "world/WorldMarkerOverlays.hpp"
#include "world/WorldCamera.hpp"
#include "aur/autotile.hpp"
#include "aur/clock.hpp"
#include "aur/world_topology.hpp"

#include <QComboBox>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
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
        owner_->paintRepeatMask(painter);  // WP1 margin mask, no-op when repeat
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
        coarse_cache = {};
        coarse_cache_map.clear();
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
                                   const QWidget* widget, qreal zoom, QPointF pan,
                                   bool repeat_view_x) {
    studio::WorldCameraSpec spec;
    const auto map_view = world::MapView::current(const_cast<Project*>(project), map_id);
    if (!map_view) return spec;
    spec.map_width = map_view->width();
    spec.map_height = map_view->height();
    spec.wraps_x = map_view->wrapsX();
    spec.repeat_view_x = repeat_view_x;
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

void WorldMapCanvasWidget::setRepeatWorldView(bool enabled) {
    if (repeat_world_view == enabled) return;
    repeat_world_view = enabled;
    constrainWorldCamera();
    refreshCanvas();
}

void WorldMapCanvasWidget::setZoom(qreal zoom) {
    fit_pending = false;
    zoom_factor = studio::WorldCamera::clampZoom(zoom);
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
}

void WorldMapCanvasWidget::fitToView() {
    if (!isVisible()) {
        // The canvas has no real size yet; the fit waits for the first
        // show/resize, or the world opens zoomed out against a stale size.
        fit_pending = true;
        return;
    }
    applyFit();
}

void WorldMapCanvasWidget::applyFit() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset,
                                 repeat_world_view);
    pan_offset = {};
    setZoom(studio::WorldCamera::fitZoom(spec, size()));
}

void WorldMapCanvasWidget::requestPendingFit() {
    if (!fit_pending) return;
    QTimer::singleShot(0, this, [this] {
        if (!fit_pending || !isVisible()) return;
        fit_pending = false;
        applyFit();
    });
}

void WorldMapCanvasWidget::zoomByFactor(qreal factor) {
    applyFit();
    setZoom(zoom_factor * factor);
}

void WorldMapCanvasWidget::recordEntityEdit() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) map["entities"] = JsonArray{};
    history.record_entity(map["entities"]);
}

QRectF WorldMapCanvasWidget::mapBounds() const {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return {};
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset,
                                 repeat_world_view);
    return studio::WorldCamera::mapBounds(spec, size());
}

void WorldMapCanvasWidget::constrainWorldCamera() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset,
                                 repeat_world_view);
    studio::WorldCameraState state{zoom_factor, pan_offset};
    studio::WorldCamera::constrain(state, spec);
    pan_offset = state.pan;
}

QPoint WorldMapCanvasWidget::tileAt(const QPointF& position) const {
    if (!project || !project->maps.contains(current_map)) return {-1, -1};
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset,
                                 repeat_world_view);
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
        // WP0.5: strength as a fraction of the 0..65535 R16 elevation range
        // (default 1.5% ≈ 983 R16 units ≈ 15 of the 0..1000 display range),
        // scaled by a soft circular falloff from the stamp centre.
        const double height_strength = height_strength_setting;
        const qreal radius_f = std::max(1, radius);
        for (const QPoint& position : stamp) {
            const int index = position.y() * world_model->width() + position.x();
            if (stroke.active && stroke.captured_cells.insert(index).second) {
                stroke.model_rects.push_back(world_model->captureRect(
                    editor::WorldModel::Layer::Height, {position.x(), position.y(), 1, 1}));
            }
            int dx = std::abs(position.x() - center_x);
            if (wraps_x) dx = std::min(dx, map_width - dx);
            const qreal dist = std::hypot(
                static_cast<qreal>(dx),
                static_cast<qreal>(position.y() - tile.y()));
            const qreal falloff = std::clamp(
                1.0 - 0.7 * (dist / radius_f), 0.3, 1.0);
            const int step = static_cast<int>(std::lround(
                65535.0 * height_strength * falloff));
            const std::uint16_t oldHeight =
                world_model->heights()[static_cast<std::size_t>(index)];
            const int delta = selected_tool == WorldTool::RaiseHeight ? step : -step;
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
    refreshCanvas();
}

void WorldMapCanvasWidget::paintRepeatMask(QPainter& painter) {
    if (!(world_model && terrain_renderer && terrain_renderer->isVisible())) return;
    if (repeat_world_view) return;
    const QRectF bounds = mapBounds();
    if (bounds.isEmpty()) return;
    if (bounds.left() > 0.0)
        painter.fillRect(QRectF(0.0, 0.0, bounds.left(), height()), QColor("#171b20"));
    if (bounds.right() < width())
        painter.fillRect(QRectF(bounds.right(), 0.0, width() - bounds.right(), height()),
                         QColor("#171b20"));
}

void WorldMapCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#171b20"));
}

void WorldMapCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (terrain_renderer) terrain_renderer->setGeometry(rect());
    if (overlay_widget) overlay_widget->setGeometry(rect());
    requestPendingFit();
    refreshCanvas();
}

void WorldMapCanvasWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    requestPendingFit();
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
    const bool hd_map = world::has_hd_surface(map);
    // Close in, the fine surface; far out, the same terrain at one pixel
    // per tile. Both are cached, so crossing over is free.
    const bool hd_surface = world::useHdSurface(map, bounds.width());
    QImage surface;
    if (hd_map) {
        QImage& cache = hd_surface ? surface_cache : coarse_cache;
        std::string& cache_map = hd_surface ? surface_cache_map : coarse_cache_map;
        if (cache_map != current_map || cache.isNull()) {
            cache = world::build_hd_surface(map, project, QRect(), hd_surface ? 0 : 1);
            cache_map = current_map;
        }
        surface = cache;
    }
    const bool wraps_x = aur::wraps_world_x(map);
    if (bounds.isEmpty() || !map.contains("layers") || !map["layers"].is_object() ||
        (!hd_map && !map["layers"].contains("ground"))) return;

    const JsonArray* ground = map["layers"].contains("ground") &&
        map["layers"]["ground"].is_array()
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
        if (hd_map && elevation_rows && y >= 0 &&
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
    // WP1: with repeat off, tile iteration stays within the single copy;
    // data lookups still wrap (wraps_x), drawing just never goes past the
    // map bounds because the camera clamps.
    const bool repeat_x = wraps_x && repeat_world_view;
    const int first_x = repeat_x ? first_visible_x
        : std::clamp(first_visible_x, 0, map_width);
    const int last_x = repeat_x ? last_visible_x
        : std::clamp(last_visible_x, 0, map_width);
    const int first_y = std::clamp(static_cast<int>(std::floor((0.0 - bounds.top()) / cell)),
                                   0, map_height);
    const int last_y = std::clamp(static_cast<int>(std::ceil((height() - bounds.top()) / cell)),
                                  0, map_height);

    if (!gpuTerrain) painter.fillRect(bounds.adjusted(-1, -1, 1, 1), QColor("#080a0c"));
    world::TerrainPaintContext terrain{painter,        &map,      bounds, QRectF(rect()),
                                     cell,            map_width, map_height,
                                     first_x,         last_x,    first_y, last_y,
                                     wraps_x,         repeat_x,  surface, ground,
                                     &groups,         tileset,   tileset_id,
                                     [this, tileset_id](int tile) { return tileImage(tileset_id, tile); }};
    world::draw_terrain(terrain);

    const JsonArray* biome_rows = map["layers"].contains("biomes") &&
        map["layers"]["biomes"].is_array()
        ? &map["layers"]["biomes"].as_array() : nullptr;
    const JsonArray* temperature_rows = map["layers"].contains("temperature") &&
        map["layers"]["temperature"].is_array()
        ? &map["layers"]["temperature"].as_array() : nullptr;

    if (!hd_map && biome_rows) {
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
            bounds, cell, map_width, first_x, last_x, first_y, last_y, repeat_x};
        world::draw_temperature_overlay(temperature_ctx);
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
        world::ClimateAxesContext axes_ctx{painter, project, &map, bounds, QRectF(rect()), cell, repeat_x};
        world::draw_climate_axes(axes_ctx);

        world::FlowOverlayContext flow_ctx{painter, project, &map, bounds, cell, map_width, map_height,
            first_x, last_x, first_y, last_y, repeat_x, currents_overlay,
            simulation_phase, simulation_timer.isActive(), is_ocean_at};
        world::draw_flow_overlay(flow_ctx);
    }

    if (weather_overlay && cell > 0) {
        world::WeatherOverlayContext weather_ctx{painter, project,
            &map, temperature_rows, biome_rows, bounds, QRectF(rect()), cell,
            map_width, map_height, first_x, last_x, repeat_x, worldgen_seed,
            simulation_phase, simulation_timer.isActive(), preview_hour,
            preview_weather, is_ocean_at};
        world::draw_weather_overlay(weather_ctx);
    }

    if (daynight_overlay && (!world_model || editor::isHeadlessRhi())) {
        world::DayNightContext daynight_ctx{painter, project, bounds, QRectF(rect()), repeat_x, preview_hour};
        world::draw_daynight_fallback(daynight_ctx);
    }

    world::EntityMarkersContext markers_ctx{painter, project,
        map.contains("entities") && map["entities"].is_array()
            ? &map["entities"].as_array() : nullptr,
        bounds, QRectF(rect()), cell, zoom_factor, map_width, map_height,
        selected_node, repeat_x};
    world::draw_entity_markers(markers_ctx);

    world::HoverContext hover_ctx{painter, bounds, cell, hover_tile.x(),
        hover_tile.y(), map_width, map_height, static_cast<qreal>(width()), repeat_x};
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

} // namespace aur
