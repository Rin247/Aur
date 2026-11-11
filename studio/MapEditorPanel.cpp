#include "MapEditorPanel.hpp"

#include "TileTexture.hpp"

#include "EditorDefs.hpp"

#include <QKeySequence>

#include <QMessageBox>

#include <QOverload>

#include <QShortcut>

#include <QWheelEvent>

#include <QSignalBlocker>

#include <array>

#include <algorithm>

#include <cmath>

namespace aur {

MapCanvasWidget::MapCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(512, 512);
    setMouseTracking(true);
    undo_stack = std::make_unique<QUndoStack>(this);
}

void MapCanvasWidget::setProject(Project* p) {
    project = p;
    undo_stack->clear();
    region_open = false;
    tile_cache.clear();
    update();
}

void MapCanvasWidget::setMap(const std::string& mid) {
    if (current_map != mid) {
        undo_stack->clear();
        region_open = false;
    }
    current_map = mid;
    hover_x = -1;
    hover_y = -1;
    tile_cache.clear();
    update();
}

void MapCanvasWidget::setLayer(int l) {
    current_layer = l;
    selecting_entity = false;
    update();
}

// D3.S1: topmost entity marker covering the tile, else -1.
int MapCanvasWidget::entityAt(const QPoint& tile) {
    if (!project || current_map.empty() || !project->maps.contains(current_map))
        return -1;
    const Json& map = project->maps.at(current_map);
    if (!map.contains("entities") || !map["entities"].is_array()) return -1;
    const auto& entities = map["entities"].as_array();
    for (int i = static_cast<int>(entities.size()) - 1; i >= 0; --i) {
        const Json& entity = entities[static_cast<size_t>(i)];
        if (entity["x"].as_int() == tile.x() && entity["y"].as_int() == tile.y())
            return i;
    }
    return -1;
}

void MapCanvasWidget::uniqueEntityId(Json& entity) const {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const std::string kind = entity.contains("type") && entity["type"].is_string()
        ? entity["type"].as_string() : "entity";
    const Json& map = project->maps.at(current_map);
    const auto& entities = map.contains("entities") && map["entities"].is_array()
        ? map["entities"].as_array() : JsonArray{};
    int highest = 0;
    const std::string prefix = kind + "_";
    for (const Json& other : entities) {
        if (!other.contains("id") || !other["id"].is_string()) continue;
        const std::string id = other["id"].as_string();
        if (id.rfind(prefix, 0) != 0) continue;
        try {
            highest = std::max(highest, std::stoi(id.substr(prefix.size())));
        } catch (const std::logic_error&) {}
    }
    entity["id"] = highest == 0 ? kind : prefix + std::to_string(highest + 1);
    if (kind == "spawn") entity["id"] = "spawn";  // spawn marker id stays stable
}

void MapCanvasWidget::moveSelectedEntity(const QPoint& tile) {
    Json& map = project->maps[current_map];
    Json& entity = map["entities"].as_array()[static_cast<size_t>(selected_entity_instance)];
    entity["x"] = tile.x();
    entity["y"] = tile.y();
    update();
}

void MapCanvasWidget::deleteSelectedEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    auto& entities = map["entities"].as_array();
    if (selected_entity_instance < 0 ||
        selected_entity_instance >= static_cast<int>(entities.size())) return;
    beginRegionUndo(-1, QStringLiteral("delete entity"));
    entities.erase(entities.begin() + selected_entity_instance);
    finishRegionUndo();
    selected_entity_instance = -1;
    update();
}

void MapCanvasWidget::fitToView() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return;
    const qreal fit = std::min((width() - 44.0) / (map_width * field_tile_size),
                               (height() - 44.0) / (map_height * field_tile_size));
    zoom_factor = std::clamp(fit, 0.05, 16.0);
    pan_offset = {};
    constrainCamera();
    update();
    emit zoomChanged(zoom_factor);
}

void MapCanvasWidget::centerOn(const QPointF& position) {
    pan_offset += QPointF(width() * 0.5, height() * 0.5) - position;
    constrainCamera();
    update();
}

void MapCanvasWidget::zoomIn() {
    zoom_factor = std::clamp(zoom_factor * 1.25, 0.05, 16.0);
    constrainCamera();
    update();
    emit zoomChanged(zoom_factor);
}

void MapCanvasWidget::zoomOut() {
    zoom_factor = std::clamp(zoom_factor / 1.25, 0.05, 16.0);
    constrainCamera();
    update();
    emit zoomChanged(zoom_factor);
}

void MapCanvasWidget::constrainCamera() {
    const QRectF bounds = mapBounds();
    const qreal horizontal_limit = std::max<qreal>(0.0, (bounds.width() - width()) * 0.5);
    const qreal vertical_limit = std::max<qreal>(0.0, (bounds.height() - height()) * 0.5);
    pan_offset.setX(std::clamp(pan_offset.x(), -horizontal_limit, horizontal_limit));
    pan_offset.setY(std::clamp(pan_offset.y(), -vertical_limit, vertical_limit));
}

void MapCanvasWidget::setSelectedTile(int t) {
    selected_tile = t;
    selected_stamp.clear();
    selecting_entity = active_tool == 2;
}

void MapCanvasWidget::setSelectedStamp(std::vector<int> tiles) {
    if (tiles.empty()) return;
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
    selected_stamp = std::move(tiles);
    selected_tile = selected_stamp.front();
    selecting_entity = active_tool == 2;
}

void MapCanvasWidget::setSelectedEntityKind(int entity_kind) {
    selected_entity_index = entity_kind;
    selected_entity_instance = -1;  // placing a new instance
    selecting_entity = active_tool == 2;
}

void MapCanvasWidget::setTool(int tool_index) {
    active_tool = std::clamp(tool_index, 0, 3);
    selecting_entity = active_tool == 2;
    if (!selecting_entity) {
        drag_entity_index = -1;
    }
}

QRectF MapCanvasWidget::mapBounds() const {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return {};
    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return {};
    const qreal cell = std::max<qreal>(1.0, field_tile_size * zoom_factor);
    const QSizeF size(map_width * cell, map_height * cell);
    return QRectF((width() - size.width()) / 2.0 + pan_offset.x(),
                  (height() - size.height()) / 2.0 + pan_offset.y(),
                  size.width(), size.height());
}

QPoint MapCanvasWidget::tileAt(const QPointF& position) const {
    const QRectF bounds = mapBounds();
    if (!bounds.contains(position) || !project || !project->maps.contains(current_map)) return {-1, -1};
    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return {-1, -1};
    const qreal cell = bounds.width() / map_width;
    const int tile_x = static_cast<int>(std::floor((position.x() - bounds.left()) / cell));
    const int tile_y = static_cast<int>(std::floor((position.y() - bounds.top()) / cell));
    if (tile_x < 0 || tile_x >= map_width || tile_y < 0 || tile_y >= map_height) return {-1, -1};
    return {tile_x, tile_y};
}

QImage MapCanvasWidget::tileImage(const std::string& tileset_id, int tile_index) {
    const auto key = std::make_pair(tileset_id, tile_index);
    const auto found = tile_cache.find(key);
    if (found != tile_cache.end()) return found->second;
    return tile_cache.emplace(key, make_tileset_tile(*project, tileset_id, tile_index)).first->second;
}

void MapCanvasWidget::paintTile(const QPoint& tile) {
    if (!project || tile.x() < 0 || tile.y() < 0 || !project->maps.contains(current_map)) return;
    // Tools 0 (Select/Move) and 3 (Eyedropper) must not paint tiles.
    if (active_tool == 0 || active_tool == 3) return;
    if (active_tool == 1) {
        if (current_layer < 0 || current_layer > 2) return;
        Json& map = project->maps[current_map];
        const std::string layer = current_layer == 0 ? "ground" :
                                  current_layer == 1 ? "objects" : "above";
        if (!map.contains("layers") || !map["layers"].is_object() ||
            !map["layers"].contains(layer) || !map["layers"][layer].is_array()) return;
        auto& rows = map["layers"][layer].as_array();
        if (tile.y() >= static_cast<int>(rows.size()) ||
            !rows[static_cast<size_t>(tile.y())].is_array()) return;
        auto& row = rows[static_cast<size_t>(tile.y())].as_array();
        if (tile.x() >= static_cast<int>(row.size()) ||
            row[static_cast<size_t>(tile.x())].as_int() < 0) return;
        beginRegionUndo(current_layer, QStringLiteral("erase tile"));
        row[static_cast<size_t>(tile.x())] = -1;
        finishRegionUndo();
        update();
        return;
    }
    if (selecting_entity && active_tool == 2) {
        Json& map = project->maps[current_map];
        if (!map.contains("entities") || !map["entities"].is_array()) map["entities"] = JsonArray{};
        Json& entities = map["entities"];
        if (selected_entity_index < 0 ||
            selected_entity_index >= static_cast<int>(editor::kEntityKinds.size())) return;
        const char* kind = editor::kEntityKinds[static_cast<size_t>(selected_entity_index)];
        auto& entity_list = entities.as_array();
        beginRegionUndo(-1, selected_entity_instance >= 0
                                ? QStringLiteral("move entity")
                                : QStringLiteral("place entity"));
        if (selected_entity_instance >= 0 &&
            selected_entity_instance < static_cast<int>(entity_list.size())) {
            // Move the selected instance (D3.S1).
            Json& entity = entity_list[static_cast<size_t>(selected_entity_instance)];
            entity["x"] = tile.x();
            entity["y"] = tile.y();
        } else {
            // Every kind is multi-instance now; spawn keeps its marker id.
            Json entity = JsonObject{{"id", kind}, {"type", kind},
                                    {"x", tile.x()}, {"y", tile.y()}};
            if (std::string(kind) == "spawn") {
                entity["dir"] = "down";
                entity["facing"] = "down";  // the inspector reads/writes "facing"
            } else {
                uniqueEntityId(entity);
            }
            entity_list.push_back(std::move(entity));
            selected_entity_instance = static_cast<int>(entity_list.size()) - 1;
            emit entitySelected(selected_entity_instance);
        }
        finishRegionUndo();
        update();
        return;
    }
    if (current_layer < 0 || current_layer > 2) return;
    Json& map = project->maps[current_map];
    if (tile.x() >= map["w"].as_int() || tile.y() >= map["h"].as_int() ||
        !map.contains("layers") || !map["layers"].is_object()) return;
    const std::string layer = current_layer == 0 ? "ground" :
                              current_layer == 1 ? "objects" : "above";
    if (!map["layers"].contains(layer) || !map["layers"][layer].is_array()) return;
    auto& rows = map["layers"][layer].as_array();
    if (tile.y() >= static_cast<int>(rows.size()) || !rows[static_cast<size_t>(tile.y())].is_array()) return;
    auto& row = rows[static_cast<size_t>(tile.y())].as_array();
    if (tile.x() >= static_cast<int>(row.size())) return;
    if (row[static_cast<size_t>(tile.x())].as_int() == selected_tile) return;
    // One undo step per stroke, not per tile (D4.S1b).
    if (!stroke_active) {
        beginRegionUndo(current_layer, QStringLiteral("paint tiles"));
        stroke_active = true;
    }
    // D2.S1: multi-tile stamps paint a square block row-major from the top-left.
    if (selected_stamp.size() > 1) {
        const int side = static_cast<int>(
            std::lround(std::sqrt(static_cast<double>(selected_stamp.size()))));
        for (int dy = 0; dy < side; ++dy) {
            const int y = tile.y() + dy;
            if (y >= static_cast<int>(rows.size()) ||
                !rows[static_cast<size_t>(y)].is_array()) break;
            auto& stamp_row = rows[static_cast<size_t>(y)].as_array();
            for (int dx = 0; dx < side; ++dx) {
                const int x = tile.x() + dx;
                if (x >= static_cast<int>(stamp_row.size())) break;
                stamp_row[static_cast<size_t>(x)] =
                    selected_stamp[static_cast<size_t>(dy * side + dx)];
            }
        }
    } else {
        row[static_cast<size_t>(tile.x())] = selected_tile;
    }
    update();
}

Json* MapCanvasWidget::editableLayer(int layer_index) {
    if (!project || current_map.empty() || layer_index < 0 || layer_index > 2 ||
        !project->maps.contains(current_map)) return nullptr;
    Json& layers = project->maps[current_map]["layers"];
    const std::string layer = layer_index == 0 ? "ground" :
                              layer_index == 1 ? "objects" : "above";
    return layers.contains(layer) ? &layers[layer] : nullptr;
}

// Snapshots the region before the write; finishRegionUndo() captures it after
// and pushes. D4.S1b2 replaces the stored whole-region copy with cell diffs.
void MapCanvasWidget::beginRegionUndo(int layer_index, const QString& label) {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    Json* target = layer_index == -1
        ? &project->maps[current_map]["entities"] : editableLayer(layer_index);
    if (!target) return;
    editor::RegionKey key;
    key.kind = layer_index == -1 ? editor::RegionKey::MapEntities
                                 : editor::RegionKey::MapLayer;
    key.index = layer_index;
    region_key = key;
    region_label = label;
    region_before = *target;
    region_open = true;
}

void MapCanvasWidget::finishRegionUndo() {
    if (!region_open || !project || current_map.empty() ||
        !project->maps.contains(current_map)) {
        region_open = false;
        return;
    }
    Json* target = region_key.kind == editor::RegionKey::MapEntities
        ? &project->maps[current_map]["entities"] : editableLayer(region_key.index);
    region_open = false;
    if (!target) return;
    auto command = std::make_unique<editor::SnapshotCommand>(region_label);
    command->setRegion(region_key);
    command->setDocument(&project->maps[current_map]);
    command->setBefore(region_before);
    command->setAfter(*target);
    undo_stack->push(command.release());
}

void MapCanvasWidget::undo() {
    if (undo_stack->canUndo()) {
        undo_stack->undo();
        tile_cache.clear();
        update();
    }
}

void MapCanvasWidget::redo() {
    if (undo_stack->canRedo()) {
        undo_stack->redo();
        tile_cache.clear();
        update();
    }
}

void MapCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.fillRect(rect(), QColor("#171b20"));

    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        painter.setPen(QColor("#c1c8ce"));
        painter.drawText(rect(), Qt::AlignCenter, "Select a map to inspect terrain and units");
        return;
    }

    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    const std::string tileset_id = map["tileset"].as_string();
    const QRectF bounds = mapBounds();
    if (bounds.isEmpty() || !map.contains("layers") || !map["layers"].is_object()) return;
    const qreal cell = bounds.width() / map_width;
    painter.fillRect(QRectF(rect()), QColor("#171b20"));
    painter.fillRect(bounds.intersected(QRectF(rect())).adjusted(-1, -1, 1, 1), QColor("#090b0e"));

    const int first_x = std::clamp(static_cast<int>(std::floor(-bounds.left() / cell)), 0, map_width);
    const int last_x = std::clamp(static_cast<int>(std::ceil((width() - bounds.left()) / cell)), 0, map_width);
    const int first_y = std::clamp(static_cast<int>(std::floor(-bounds.top() / cell)), 0, map_height);
    const int last_y = std::clamp(static_cast<int>(std::ceil((height() - bounds.top()) / cell)), 0, map_height);

    auto draw_layer = [&](const char* layer_name, qreal opacity) {
        if (!map["layers"].contains(layer_name) || !map["layers"][layer_name].is_array()) return;
        const auto& rows = map["layers"][layer_name].as_array();
        painter.save();
        painter.setOpacity(opacity);
        for (int y = first_y; y < last_y && y < static_cast<int>(rows.size()); ++y) {
            if (!rows[static_cast<size_t>(y)].is_array()) continue;
            const auto& row = rows[static_cast<size_t>(y)].as_array();
            for (int x = first_x; x < last_x && x < static_cast<int>(row.size()); ++x) {
                const int tile_index = row[static_cast<size_t>(x)].as_int();
                if (tile_index < 0) continue;
                const QRectF target(bounds.left() + x * cell, bounds.top() + y * cell, cell, cell);
                const QImage tile = tileImage(tileset_id, tile_index);
                if (tile.isNull()) {
                    painter.fillRect(target, QColor("#34404a"));
                    painter.setPen(QColor("#c1c8ce"));
                    painter.drawText(target, Qt::AlignCenter, QString::number(tile_index));
                } else {
                    painter.drawImage(target, tile);
                }
            }
        }
        painter.restore();
    };

    draw_layer("ground", 1.0);
    draw_layer("objects", 0.95);
    draw_layer("above", 0.82);

    if (current_layer == 3 && map.contains("layers") && map["layers"].is_object()) {
        const Json* ground = map["layers"].contains("ground") && map["layers"]["ground"].is_array()
            ? &map["layers"]["ground"] : nullptr;
        const Json* objects = map["layers"].contains("objects") && map["layers"]["objects"].is_array()
            ? &map["layers"]["objects"] : nullptr;
        painter.save();
        for (int y = first_y; y < last_y; ++y) {
            const auto tile_at = [y, map_width](const Json* layer, int x) {
                if (!layer || y >= static_cast<int>(layer->as_array().size())) return -1;
                const Json& row = (*layer)[static_cast<size_t>(y)];
                if (!row.is_array() || x >= static_cast<int>(row.as_array().size())) return -1;
                return row[static_cast<size_t>(x)].as_int();
            };
            for (int x = first_x; x < last_x; ++x) {
                const int ground_tile = tile_at(ground, x);
                const int object_tile = tile_at(objects, x);
                const int flags = project->tile_flags(tileset_id, ground_tile) |
                                  project->tile_flags(tileset_id, object_tile);
                QColor color("#00000000");
                for (const auto& style : editor::kFlagStyles) {
                    if (flags & style.flag) {
                        color = style.color;
                        break;
                    }
                }
                if (color.alpha() > 0) {
                    painter.fillRect(QRectF(bounds.left() + x * cell, bounds.top() + y * cell, cell, cell), color);
                }
            }
        }
        painter.restore();
    }

    if (map.contains("mode") && map["mode"].as_string() == "tactics" &&
        hover_x >= 0 && hover_y >= 0) {
        for (int y = std::max(first_y, hover_y - 3); y < std::min(last_y, hover_y + 4); ++y) {
            for (int x = std::max(first_x, hover_x - 3); x < std::min(last_x, hover_x + 4); ++x) {
                const int distance = std::abs(x - hover_x) + std::abs(y - hover_y);
                if (distance == 0 || distance > 3) continue;
                QColor range = distance <= 2 ? QColor("#3f7de8") : QColor("#d95151");
                range.setAlpha(85);
                painter.fillRect(QRectF(bounds.left() + x * cell, bounds.top() + y * cell, cell, cell), range);
            }
        }
    }

    painter.setPen(QPen(QColor(13, 18, 22, 190), 0.7));
    for (int x = first_x; x <= last_x; ++x) {
        const qreal px = bounds.left() + x * cell;
        painter.drawLine(QPointF(px, bounds.top()), QPointF(px, bounds.bottom()));
    }
    for (int y = first_y; y <= last_y; ++y) {
        const qreal py = bounds.top() + y * cell;
        painter.drawLine(QPointF(bounds.left(), py), QPointF(bounds.right(), py));
    }

    if (map.contains("entities") && map["entities"].is_array()) {
        const auto& entities = map["entities"].as_array();
        for (size_t entity_index = 0; entity_index < entities.size(); ++entity_index) {
            const Json& entity = entities[entity_index];
            const int x = entity["x"].as_int();
            const int y = entity["y"].as_int();
            if (x < 0 || y < 0 || x >= map_width || y >= map_height) continue;
            const std::string type = entity.contains("type") ? entity["type"].as_string() : "";
            QColor color("#ffb74d");
            QString label("?");
            if (type == "spawn") { color = QColor("#43a047"); label = "S"; }
            else if (type == "exit") { color = QColor("#ab47bc"); label = "X"; }
            else if (type == "npc") { color = QColor("#ffb74d"); label = "N"; }
            else if (type == "chest") { color = QColor("#8d6e63"); label = "C"; }
            else if (type == "sign") { color = QColor("#78909c"); label = "!"; }
            else if (type == "trigger") { color = QColor("#ec407a"); label = "T"; }
            else if (type == "light") { color = QColor("#fdd835"); label = "L"; }
            else if (type == "enemy") { color = QColor("#e53935"); label = "E"; }
            const bool selected = selecting_entity &&
                entity_index == static_cast<size_t>(selected_entity_instance);
            const QRectF cell_rect(bounds.left() + x * cell, bounds.top() + y * cell, cell, cell);
            const qreal marker_size = std::clamp(cell * 0.7, 8.0, 24.0);
            const QRectF marker(cell_rect.center().x() - marker_size / 2,
                                cell_rect.center().y() - marker_size / 2,
                                marker_size, marker_size);
            painter.setPen(QPen(selected ? QColor("#fff3c4") : QColor("#171b20"), selected ? 2.5 : 1.2));
            painter.setBrush(color);
            painter.drawRoundedRect(marker, 3, 3);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Segoe UI", 9, QFont::Bold));
            painter.drawText(marker, Qt::AlignCenter, label);
        }
    }

    if (hover_x >= first_x && hover_x < last_x && hover_y >= first_y && hover_y < last_y) {
        const QRectF hover(bounds.left() + hover_x * cell, bounds.top() + hover_y * cell, cell, cell);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(hover);
    }

    painter.setPen(QColor("#c1c8ce"));
    painter.drawText(QRectF(16, 8, width() - 32, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     QString("%1 × %2 · %3")
                         .arg(map_width).arg(map_height)
                         .arg(QString::fromStdString(tileset_id)));
}

void MapCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning = true;
        pan_start_position = event->position();
        pan_start_offset = pan_offset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        stroke_active = false;
        const QPoint tile = tileAt(event->position());
        // Tool 0 (Select/Move): click an entity to select; drag moves it.
        if (active_tool == 0) {
            const int hit = entityAt(tile);
            if (hit >= 0) {
                selected_entity_instance = hit;
                drag_entity_index = hit;
                beginRegionUndo(-1, QStringLiteral("move entity"));
                stroke_active = true; // finishRegionUndo fires at release
                emit entitySelected(hit);
                update();
                event->accept();
                return;
            }
            drag_entity_index = -1;
            if (selected_entity_instance >= 0) {
                selected_entity_instance = -1;
                emit entitySelected(-1);
                update();
            }
            event->accept();
            return;
        }
        Json* layer = editableLayer(current_layer);
        // Arm the stroke only for the erase tool; paint and entity placement
        // take their snapshots elsewhere.
        const bool tile_tool = active_tool == 1;
        if (layer && tile.x() >= 0 && tile.y() >= 0 && tile_tool &&
            tile.y() < static_cast<int>(layer->as_array().size()) &&
            tile.x() < static_cast<int>((*layer)[static_cast<size_t>(tile.y())].as_array().size())) {
            beginRegionUndo(current_layer, QStringLiteral("paint tiles"));
            stroke_active = true;
        }
        paintTile(tile);
    }
}

void MapCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning && event->buttons().testFlag(Qt::MiddleButton)) {
        pan_offset = pan_start_offset + event->position() - pan_start_position;
        constrainCamera();
        update();
        return;
    }
    const QPoint tile = tileAt(event->position());
    if (tile.x() != hover_x || tile.y() != hover_y) {
        hover_x = tile.x();
        hover_y = tile.y();
        update();
    }
    // D3.S1: drag the selected entity with tool 0.
    if (drag_entity_index >= 0 && event->buttons().testFlag(Qt::LeftButton) &&
        tile.x() >= 0 && tile.y() >= 0) {
        moveSelectedEntity(tile);
        return;
    }
    if (stroke_active && event->buttons().testFlag(Qt::LeftButton)) paintTile(tile);
}

void MapCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (stroke_active) finishRegionUndo();
        stroke_active = false;
        drag_entity_index = -1;
    }
    if (event->button() == Qt::MiddleButton) {
        panning = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void MapCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const QPointF cursor = event->position();
    const QRectF old_bounds = mapBounds();
    const Json& map = project->maps.at(current_map);
    const qreal old_cell = old_bounds.width() / map["w"].as_int();
    if (old_cell <= 0.0) return;
    const QPointF map_position((cursor.x() - old_bounds.left()) / old_cell,
                               (cursor.y() - old_bounds.top()) / old_cell);
    const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    zoom_factor = std::clamp(zoom_factor * factor, 0.05, 16.0);
    const QRectF new_bounds = mapBounds();
    const qreal new_cell = new_bounds.width() / map["w"].as_int();
    pan_offset += cursor - QPointF(new_bounds.left() + map_position.x() * new_cell,
                                   new_bounds.top() + map_position.y() * new_cell);
    constrainCamera();
    update();
    emit zoomChanged(zoom_factor);
    event->accept();
}

void MapCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    constrainCamera();
    update();
}

void MapCanvasWidget::leaveEvent(QEvent* event) {
    if (hover_x >= 0 || hover_y >= 0) {
        hover_x = -1;
        hover_y = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

MapEditorPanel::MapEditorPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel("Map:", this));
    mapCombo = new QComboBox(this);
    mapCombo->setObjectName("mapSelector");
    toolbar->addWidget(mapCombo);

    toolbar->addWidget(new QLabel("Mode:", this));
    modeCombo = new QComboBox(this);
    modeCombo->addItems({"Real-Time Action", "Turn-Based", "Tactical"});
    modeCombo->setMinimumWidth(150);
    toolbar->addWidget(modeCombo);

    modeHint = new QLabel("", this);
    modeHint->setStyleSheet("color: #888888; font-style: italic;");
    toolbar->addWidget(modeHint);

    auto setModeHint = [this](int idx) {
        const char* h = nullptr;
        switch (idx) {
            case 0: h = "Smooth real-time exploration & combat on the field (Trials of Mana style)."; break;
            case 1: h = "Command-menu turn order on the exploration plane (Chrono Trigger style)."; break;
            case 2: h = "Grid-based positioning on the exploration plane (Fire Emblem / Triangle Strategy style)."; break;
            default: h = "";
        }
        modeHint->setText(h ? h : "");
    };
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, setModeHint](int idx) {
        if (!project || current_map.empty() || !project->maps.contains(current_map)) {
            setModeHint(idx);
            return;
        }
        const char* m = nullptr;
        switch (idx) {
            case 0: m = "action"; break;
            case 1: m = "turn"; break;
            case 2: m = "tactics"; break;
            default: m = "action";
        }
        auto& mapJson = project->maps[current_map];
        mapJson["mode"] = m;
        setModeHint(idx);
    });

    toolbar->addWidget(new QLabel("Layer:", this));
    layerCombo = new QComboBox(this);
    layerCombo->addItems({"Ground", "Objects", "Above", "Collision Overlay"});
    toolbar->addWidget(layerCombo);

    toolbar->addWidget(new QLabel("Tool:", this));
    toolCombo = new QComboBox(this);
    toolCombo->addItems({"Paint Tile", "Erase Tile", "Place Entity / Area", "Select / Move"});
    toolCombo->setToolTip("Tool (keys 1-4)");
    toolbar->addWidget(toolCombo);
    auto* undoButton = new QPushButton("Undo", this);
    auto* redoButton = new QPushButton("Redo", this);
    toolbar->addWidget(undoButton);
    toolbar->addWidget(redoButton);

    mainLayout->addLayout(toolbar);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    toolbar->addSpacing(8);
    zoomOutButton = new QPushButton("−", this);
    toolbar->addWidget(zoomOutButton);
    zoomLabel = new QLabel("100%", this);
    toolbar->addWidget(zoomLabel);
    zoomInButton = new QPushButton("+", this);
    toolbar->addWidget(zoomInButton);
    fitButton = new QPushButton("Fit", this);
    toolbar->addWidget(fitButton);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    QWidget* leftDock = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftDock);
    leftLayout->addWidget(new QLabel("Tile Palette:", this));
    tilePalette = new TilePaletteWidget(project, leftDock);
    leftLayout->addWidget(tilePalette);
    leftLayout->addWidget(new QLabel("Entity / Asset Palette:", this));

    entityTypeWidget = new QListWidget(this);
    entityTypeWidget->setObjectName("mapAssetPalette");
    const std::array<std::pair<const char*, const char*>, 8> entity_kinds{{
        {"spawn", "Spawn Point"}, {"exit", "Exit Door"}, {"npc", "NPC / Citizen"},
        {"chest", "Treasure Chest"}, {"sign", "Sign"}, {"trigger", "Trigger"},
        {"light", "Light"}, {"enemy", "Enemy"}}};
    for (const auto& [id, label] : entity_kinds) {
        auto* item = new QListWidgetItem(QString("[Entity] %1").arg(label), entityTypeWidget);
        item->setData(Qt::UserRole, QString::fromLatin1(id));
    }
    // Tile rows come from the shared TilePaletteWidget (D2.S1); the entity
    // list below keeps fixed indexes for the selected_entity_kind split.
    leftLayout->addWidget(entityTypeWidget);
    splitter->addWidget(leftDock);

    canvas = new MapCanvasWidget(this);
    canvas->setProject(project);
    splitter->addWidget(canvas);
    connect(toolCombo, &QComboBox::currentIndexChanged, canvas,
            &MapCanvasWidget::setTool);
    for (int key = 1; key <= 4; ++key) {
        QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_0 + key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key]() {
            toolCombo->setCurrentIndex(key - 1);
        });
    }
    connect(undoButton, &QPushButton::clicked, canvas, &MapCanvasWidget::undo);
    connect(redoButton, &QPushButton::clicked, canvas, &MapCanvasWidget::redo);
    connect(undoShortcut, &QShortcut::activated, canvas, &MapCanvasWidget::undo);
    connect(redoShortcut, &QShortcut::activated, canvas, &MapCanvasWidget::redo);

    connect(tilePalette, &TilePaletteWidget::stampSelected, canvas,
            [this](const std::vector<int>& tiles) {
                canvas->setTool(1);  // choosing a tile implies the paint tool
                if (toolCombo->currentIndex() != 1) toolCombo->setCurrentIndex(1);
                if (tiles.size() == 1) canvas->setSelectedTile(tiles.front());
                else canvas->setSelectedStamp(tiles);
            });
    connect(tilePalette, &TilePaletteWidget::tilePicked, canvas,
            [this](int tile_index) {
                if (tile_index >= 0) {
                    canvas->setTool(1);
                    if (toolCombo->currentIndex() != 1) toolCombo->setCurrentIndex(1);
                    canvas->setSelectedTile(tile_index);
                }
            });

    auto* inspectorDock = new QWidget(this);
    QVBoxLayout* inspectorLayout = new QVBoxLayout(inspectorDock);
    QFormLayout* form = new QFormLayout();
    auto* deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated, canvas,
            &MapCanvasWidget::deleteSelectedEntity);
    connect(entityDeleteButton, &QPushButton::clicked, canvas,
            &MapCanvasWidget::deleteSelectedEntity);
    selectedObjectLabel = new QLabel("Tile / Entity", inspectorDock);
    form->addRow("Selected Object:", selectedObjectLabel);
    idEdit = new QLineEdit(inspectorDock);
    idEdit->setReadOnly(true);
    form->addRow("Id:", idEdit);
    facingEdit = new QLineEdit("down", inspectorDock);
    form->addRow("Facing Dir:", facingEdit);
    eventCombo = new QComboBox(inspectorDock);
    eventCombo->setEditable(true);
    form->addRow("Event ID:", eventCombo);
    nameEdit = new QLineEdit(inspectorDock);
    form->addRow("Name:", nameEdit);
    textEdit = new QLineEdit(inspectorDock);
    form->addRow("Text:", textEdit);
    paramSpin = new QSpinBox(inspectorDock);
    paramSpin->setRange(-1000000, 1000000);
    form->addRow("Param:", paramSpin);
    inspectorLayout->addLayout(form);
    eventApplyButton = new QPushButton("Apply to Entity", inspectorDock);
    inspectorLayout->addWidget(eventApplyButton);
    entityDeleteButton = new QPushButton("Delete Entity (Del)", inspectorDock);
    inspectorLayout->addWidget(entityDeleteButton);
    inspectorLayout->addStretch();
    splitter->addWidget(inspectorDock);

    splitter->setSizes({200, 700, 300});
    mainLayout->addWidget(splitter, 1);

    connect(mapCombo, &QComboBox::currentIndexChanged, [this](int idx) {
        if (!project) return;
        if (idx < 0 || idx >= static_cast<int>(project->maps.size())) return;
        auto it = std::next(project->maps.begin(), idx);
        current_map = it->first;
        canvas->setMap(current_map);
        canvas->fitToView();
        refreshTilePalette();
        // Sync mode selector to the loaded map's mode.
        if (modeCombo && modeHint) {
            auto& m = it->second;
            std::string mode = m.contains("mode") ? m["mode"].as_string() : "action";
            modeCombo->blockSignals(true);
            if (mode == "turn") modeCombo->setCurrentIndex(1);
            else if (mode == "tactics") modeCombo->setCurrentIndex(2);
            else modeCombo->setCurrentIndex(0);
            modeCombo->blockSignals(false);
            const char* h = nullptr;
            if (mode == "action") h = "Smooth real-time exploration & combat on the field (Trials of Mana style).";
            else if (mode == "turn") h = "Command-menu turn order on the exploration plane (Chrono Trigger style).";
            else if (mode == "tactics") h = "Grid-based positioning on the exploration plane (Fire Emblem / Triangle Strategy style).";
            modeHint->setText(h ? h : "");
        }
    });

    connect(layerCombo, &QComboBox::currentIndexChanged, [this](int idx) {
        canvas->setLayer(idx);
    });
    connect(zoomInButton, &QPushButton::clicked, canvas, [this]() { canvas->zoomIn(); });
    connect(zoomOutButton, &QPushButton::clicked, canvas, [this]() { canvas->zoomOut(); });
    connect(fitButton, &QPushButton::clicked, canvas, [this]() { canvas->fitToView(); });
    connect(canvas, &MapCanvasWidget::zoomChanged, this, [this](qreal zoom) {
        zoomLabel->setText(QString("%1%").arg(qRound(zoom * 100)));
    });

    connect(entityTypeWidget, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const QVariant data = entityTypeWidget->item(row)->data(Qt::UserRole);
        if (data.typeId() == QMetaType::Int) {
            canvas->setSelectedTile(data.toInt());
        } else {
            const QString type = data.toString();
            const auto found = std::find_if(editor::kEntityKinds.begin(), editor::kEntityKinds.end(),
                                            [&type](const char* kind) {
                                                return type == QLatin1String(kind);
                                            });
            if (found != editor::kEntityKinds.end()) {
                const int entity_kind = static_cast<int>(std::distance(editor::kEntityKinds.begin(), found));
                canvas->setSelectedEntityKind(entity_kind);
                emit canvas->entitySelectionRequested(entity_kind);
            }
        }
    });
    connect(canvas, &MapCanvasWidget::entitySelectionRequested, this, [this](int entity_kind) {
        if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
        const auto& map = project->maps.at(current_map);
        if (!map.contains("entities") || !map["entities"].is_array()) return;
        static const std::array<const char*, 8> kinds{
            "spawn", "exit", "npc", "chest", "sign", "trigger", "light", "enemy"};
        if (entity_kind < 0 || entity_kind >= static_cast<int>(kinds.size())) return;
        const char* wanted = kinds[static_cast<size_t>(entity_kind)];
        const auto found = std::find_if(map["entities"].as_array().begin(),
            map["entities"].as_array().end(), [wanted](const Json& entity) {
                return entity.contains("type") && entity["type"].is_string() &&
                       entity["type"].as_string() == wanted;
            });
        if (found == map["entities"].as_array().end()) return;
        const QRectF bounds = canvas->mapBounds();
        const QPointF center = bounds.topLeft() + QPointF(
            (found->operator[]("x").as_int() + 0.5) * bounds.width() / map["w"].as_int(),
            (found->operator[]("y").as_int() + 0.5) * bounds.height() / map["h"].as_int());
        canvas->centerOn(center);
    });

    // Inspector: edits the clicked/selected entity instance (D3.S1), not
    // "first of kind". Kind selection in the palette only arms placement.
    syncInspectorFromInstance = [this](int instance) {
        const auto disable = [this]() {
            selectedObjectLabel->setText("Tile / Entity");
            idEdit->clear();
            facingEdit->clear();
            facingEdit->setEnabled(false);
            eventCombo->clear();
            eventCombo->setEnabled(false);
            nameEdit->clear();
            nameEdit->setEnabled(false);
            textEdit->clear();
            textEdit->setEnabled(false);
            paramSpin->setValue(0);
            paramSpin->setEnabled(false);
            eventApplyButton->setEnabled(false);
            entityDeleteButton->setEnabled(false);
        };
        const Json* entity = nullptr;
        if (project && !current_map.empty() &&
            project->maps.contains(current_map) && instance >= 0) {
            const auto& map = project->maps.at(current_map);
            if (map.contains("entities") && map["entities"].is_array() &&
                instance < static_cast<int>(map["entities"].as_array().size()))
                entity = &map["entities"].as_array()[static_cast<size_t>(instance)];
        }
        if (!entity) {
            disable();
            return;
        }
        const auto str_field = [&entity](const char* key) {
            return entity->contains(key) && (*entity)[key].is_string()
                ? QString::fromStdString((*entity)[key].as_string()) : QString();
        };
        selectedObjectLabel->setText(QString("Entity #%1: %2")
            .arg(instance)
            .arg(str_field("type")));
        idEdit->setText(str_field("id"));
        facingEdit->setText(entity->contains("facing") || entity->contains("dir")
            ? (entity->contains("facing") && (*entity)["facing"].is_string()
                   ? str_field("facing") : str_field("dir"))
            : "down");
        facingEdit->setEnabled(true);
        eventCombo->setCurrentText(str_field("event"));
        eventCombo->setEnabled(true);
        nameEdit->setText(str_field("name"));
        nameEdit->setEnabled(true);
        textEdit->setText(str_field("text"));
        textEdit->setEnabled(true);
        paramSpin->setValue(entity->contains("param") && (*entity)["param"].is_number()
            ? (*entity)["param"].as_int() : 0);
        paramSpin->setEnabled(true);
        eventApplyButton->setEnabled(true);
        entityDeleteButton->setEnabled(true);
    };
    connect(canvas, &MapCanvasWidget::entitySelected, this,
            [this](int instance) { syncInspectorFromInstance(instance); });
    connect(eventApplyButton, &QPushButton::clicked, this, [this]() {
        if (!project || current_map.empty() || !project->maps.contains(current_map))
            return;
        Json& map = project->maps[current_map];
        if (!map.contains("entities") || !map["entities"].is_array()) return;
        auto& entities = map["entities"].as_array();
        const int instance = canvas->selectedEntityInstance();
        if (instance < 0 || instance >= static_cast<int>(entities.size())) return;
        Json& entity = entities[static_cast<size_t>(instance)];
        const std::string kind = entity.contains("type") && entity["type"].is_string()
            ? entity["type"].as_string() : std::string();
        const std::string event_id = eventCombo->currentText().toStdString();
        if (!event_id.empty() && !project->events.contains(event_id)) {
            QMessageBox::warning(this, "Event ID",
                                 QString("Event '%1' is not in the project event registry.")
                                     .arg(QString::fromStdString(event_id)));
            return;
        }
        entity["facing"] = facingEdit->text().toStdString();
        if (kind == "spawn") entity["dir"] = facingEdit->text().toStdString();
        if (event_id.empty()) entity.as_object().erase("event");
        else entity["event"] = event_id;
        const std::string name = nameEdit->text().toStdString();
        if (name.empty()) entity.as_object().erase("name");
        else entity["name"] = name;
        const std::string text = textEdit->text().toStdString();
        if (text.empty()) entity.as_object().erase("text");
        else entity["text"] = text;
        if (paramSpin->value() != 0) entity["param"] = paramSpin->value();
        else entity.as_object().erase("param");
        selectedObjectLabel->setText(QString("Entity #%1 (%2 saved)")
                                         .arg(instance).arg(QString::fromStdString(kind)));
    });
    const auto refreshEventChoices = [this]() {
        if (!eventCombo) return;
        const QSignalBlocker blocker(eventCombo);
        eventCombo->clear();
        eventCombo->addItem(QString());
        if (project)
            for (const auto& [event_id, event] : project->events)
                eventCombo->addItem(QString::fromStdString(event_id));
    };
    refreshEventChoices();

    refreshMaps();
}

void MapEditorPanel::setProject(Project* p) {
    project = p;
    if (canvas) canvas->setProject(p);
    refreshMaps();
}

void MapEditorPanel::refreshMaps() {
    if (!mapCombo) return;
    mapCombo->blockSignals(true);
    mapCombo->clear();
    if (project) {
        for (const auto& [mid, m] : project->maps) {
            std::string label = mid;
            if (m.contains("name")) label += " - " + m["name"].as_string();
            mapCombo->addItem(QString::fromStdString(label));
        }
        if (!project->maps.empty()) {
            canvas->setMap(project->maps.begin()->first);
            canvas->fitToView();
            current_map = project->maps.begin()->first;
            refreshTilePalette();
            if (modeCombo && modeHint) {
                auto& m = project->maps.begin()->second;
                std::string mode = m.contains("mode") ? m["mode"].as_string() : "action";
                modeCombo->blockSignals(true);
                if (mode == "turn") modeCombo->setCurrentIndex(1);
                else if (mode == "tactics") modeCombo->setCurrentIndex(2);
                else modeCombo->setCurrentIndex(0);
                modeCombo->blockSignals(false);
                const char* h = nullptr;
                if (mode == "action") h = "Smooth real-time exploration & combat on the field (Trials of Mana style).";
                else if (mode == "turn") h = "Command-menu turn order on the exploration plane (Chrono Trigger style).";
                else if (mode == "tactics") h = "Grid-based positioning on the exploration plane (Fire Emblem / Triangle Strategy style).";
                modeHint->setText(h ? h : "");
            }
        }
    }
    mapCombo->blockSignals(false);
    if (mapCombo->count() > 0) mapCombo->setCurrentIndex(0);
    else {
        current_map.clear();
        canvas->setMap({});
        refreshTilePalette();
    }
}

void MapEditorPanel::refreshTilePalette() {
    if (!tilePalette) return;
    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        tilePalette->setTileset({});
        return;
    }
    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map.contains("tileset") && map["tileset"].is_string()
        ? map["tileset"].as_string() : "";
    tilePalette->setTileset(tileset_id);
    tilePalette->refresh();
}

void MapEditorPanel::onTilesetChanged() {
    if (canvas) canvas->invalidateTileCache();
}

} // namespace aur
