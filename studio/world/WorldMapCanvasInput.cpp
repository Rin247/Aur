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
        // Strength as a fraction of the 0..65535 R16 elevation range with circular falloff.
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
    editor::WorldCommandHistory::Action redo;
    if (action.entities) {
        redo.entities = true;
        redo.state = map["entities"];
        map["entities"] = action.state;
        emit entitiesChanged();
    } else if (!action.model_rects.empty() && world_model) {
        for (auto it = action.model_rects.rbegin(); it != action.model_rects.rend(); ++it) {
            redo.model_rects.push_back(world_model->captureRect(it->layer, it->rect));
            world_model->restore(*it);
            world_model->writeRect(map, it->layer, it->rect);
        }
    } else {
        redo.entities = false;
        redo.state = map["layers"];
        map["layers"] = action.state;
    }
    history.push_redo(std::move(redo));
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::redo() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !history.can_redo()) return;
    Json& map = project->maps[current_map];
    const editor::WorldCommandHistory::Action action = history.take_redo();
    editor::WorldCommandHistory::Action undo;
    if (action.entities) {
        undo.entities = true;
        undo.state = map["entities"];
        map["entities"] = action.state;
        emit entitiesChanged();
    } else if (!action.model_rects.empty() && world_model) {
        for (auto it = action.model_rects.rbegin(); it != action.model_rects.rend(); ++it) {
            undo.model_rects.push_back(world_model->captureRect(it->layer, it->rect));
            world_model->restore(*it);
            world_model->writeRect(map, it->layer, it->rect);
        }
    } else {
        undo.entities = false;
        undo.state = map["layers"];
        map["layers"] = action.state;
    }
    history.push_undo(std::move(undo));
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
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
        constexpr std::array<qreal, 6> snapSteps{0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
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
        const qreal step = event->modifiers().testFlag(Qt::ShiftModifier) ? 1.02 : 1.06;
        const qreal factor = event->angleDelta().y() > 0 ? step : 1.0 / step;
        updated_zoom = std::clamp(zoom_factor * factor, 0.05, 16.0);
    }
    if (updated_zoom == zoom_factor) return;
    zoom_factor = updated_zoom;
    user_zoom_multiplier = zoom_factor / std::max(0.01, fitZoomForCurrentSize());
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
