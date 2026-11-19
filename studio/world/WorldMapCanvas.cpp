#include "world/WorldMapCanvas.hpp"
#include "core/RhiBackend.hpp"
#include "map/TileTexture.hpp"
#include "world/HdSurface.hpp"
#include "world/TerrainLod.hpp"
#include "world/WorldTerrainPaint.hpp"
#include "world/WorldMapHelpers.hpp"
#include "world/MapView.hpp"
#include "world/WorldWeatherOverlay.hpp"
#include "world/WorldClimateOverlays.hpp"
#include "world/WorldMarkerOverlays.hpp"
#include "world/WorldCamera.hpp"
#include "world/GlobeProjection.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"
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
    const bool gpuTerrain = world_model && terrain_renderer && !editor::isHeadlessRhi() &&
                            !globe_view;
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

qreal WorldMapCanvasWidget::fitZoomForCurrentSize() const {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return 1.0;
    const auto spec = cameraSpec(project, current_map, this, zoom_factor, pan_offset,
                                 repeat_world_view);
    return studio::WorldCamera::fitZoom(spec, size());
}

void WorldMapCanvasWidget::setZoom(qreal zoom) {
    fit_pending = false;
    zoom_factor = studio::WorldCamera::clampZoom(zoom);
    user_zoom_multiplier = zoom_factor / std::max(0.01, fitZoomForCurrentSize());
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
}

void WorldMapCanvasWidget::fitToView() {
    user_zoom_multiplier = 1.0;
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
    pan_offset = {};
    zoom_factor = studio::WorldCamera::clampZoom(fitZoomForCurrentSize() * user_zoom_multiplier);
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
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
    user_zoom_multiplier = factor;
    applyFit();
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

void WorldMapCanvasWidget::setGlobeView(bool enabled) {
    if (globe_view == enabled) return;
    globe_view = enabled;
    globe_cache = QImage();
    refreshCanvas();
}

void WorldMapCanvasWidget::paintGlobe(QPainter& painter, const QImage& surface) {
    const QSize target_size = size();
    if (target_size.isEmpty()) return;
    if (globe_cache.isNull() || globe_cache_size != target_size ||
        std::abs(globe_cache_yaw - globe_yaw) > 0.0001 ||
        std::abs(globe_cache_pitch - globe_pitch) > 0.0001) {
        const QImage source = surface.convertToFormat(QImage::Format_RGB32);
        globe_cache = QImage(target_size, QImage::Format_RGB32);
        globe_cache.fill(QColor("#0b0f14"));
        const double radius = std::min(target_size.width(), target_size.height()) * 0.46;
        const double cx = target_size.width() * 0.5;
        const double cy = target_size.height() * 0.5;
        for (int y = 0; y < target_size.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(globe_cache.scanLine(y));
            const double ny = (cy - y) / radius;
            for (int x = 0; x < target_size.width(); ++x) {
                const double nx = (x - cx) / radius;
                const world::GlobePoint point = world::projectToGlobe(nx, ny, globe_yaw, globe_pitch);
                if (!point.visible) continue;
                const int sx = std::clamp(static_cast<int>(point.u * source.width()), 0, source.width() - 1);
                const int sy = std::clamp(static_cast<int>(point.v * source.height()), 0, source.height() - 1);
                const QRgb colour = reinterpret_cast<const QRgb*>(source.constScanLine(sy))[sx];
                const double shade = world::globeShade(nx, ny);
                line[x] = qRgb(static_cast<int>(qRed(colour) * shade),
                               static_cast<int>(qGreen(colour) * shade),
                               static_cast<int>(qBlue(colour) * shade));
            }
        }
        globe_cache_size = target_size;
        globe_cache_yaw = globe_yaw;
        globe_cache_pitch = globe_pitch;
    }
    painter.drawImage(0, 0, globe_cache);
}

void WorldMapCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (terrain_renderer) terrain_renderer->setGeometry(rect());
    if (overlay_widget) overlay_widget->setGeometry(rect());
    if (isVisible() && width() > 24 && height() > 24) {
        zoom_factor = studio::WorldCamera::clampZoom(fitZoomForCurrentSize() * user_zoom_multiplier);
        constrainWorldCamera();
        emit zoomChanged(zoom_factor);
    }
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
        painter.setPen(theme::qt(theme::tokens().palette.text_dim));
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
    if (globe_view && !surface.isNull()) {
        paintGlobe(painter, surface);
        return;
    }

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
    // WP1: with repeat off, tile iteration stays within the single copy.
    const bool repeat_x = wraps_x && repeat_world_view;
    const int first_x = repeat_x ? first_visible_x
        : std::clamp(first_visible_x, 0, map_width);
    const int last_x = repeat_x ? last_visible_x
        : std::clamp(last_visible_x, 0, map_width);
    const int first_y = std::clamp(static_cast<int>(std::floor((0.0 - bounds.top()) / cell)),
                                   0, map_height);
    const int last_y = std::clamp(static_cast<int>(std::ceil((height() - bounds.top()) / cell)),
                                  0, map_height);

    if (!gpuTerrain) painter.fillRect(bounds.adjusted(-1, -1, 1, 1), theme::qt(theme::tokens().palette.divider));
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

} // namespace aur
