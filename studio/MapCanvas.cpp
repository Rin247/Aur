#include "MapCanvas.hpp"

#include "TileTexture.hpp"

#include "EditorDefs.hpp"

#include <QTimer>

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
    undo_recorder.cancel();
    tile_cache.clear();
    update();
}

void MapCanvasWidget::setMap(const std::string& mid) {
    if (current_map != mid) {
        undo_stack->clear();
        undo_recorder.cancel();
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
    beginStroke(-1, QStringLiteral("delete entity"));
    entities.erase(entities.begin() + selected_entity_instance);
    endStroke();
    selected_entity_instance = -1;
    update();
}

void MapCanvasWidget::fitToView() {
    if (!isVisible()) {
        // The canvas has no real size yet; the fit waits for the first
        // show/resize, or the map opens zoomed out against a stale size.
        fit_pending = true;
        return;
    }
    applyFit();
}

void MapCanvasWidget::applyFit() {
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

void MapCanvasWidget::requestPendingFit() {
    if (!fit_pending) return;
    QTimer::singleShot(0, this, [this] {
        if (!fit_pending || !isVisible()) return;
        fit_pending = false;
        applyFit();
    });
}

void MapCanvasWidget::centerOn(const QPointF& position) {
    pan_offset += QPointF(width() * 0.5, height() * 0.5) - position;
    constrainCamera();
    update();
}

void MapCanvasWidget::setZoomFactor(qreal zoom) {
    fit_pending = false;
    zoom_factor = std::clamp(zoom, 0.05, 16.0);
    constrainCamera();
    update();
    emit zoomChanged(zoom_factor);
}

void MapCanvasWidget::zoomByFactor(qreal factor) {
    applyFit();
    setZoomFactor(zoom_factor * factor);
}

void MapCanvasWidget::zoomIn() {
    fit_pending = false;
    zoom_factor = std::clamp(zoom_factor * 1.25, 0.05, 16.0);
    constrainCamera();
    update();
    emit zoomChanged(zoom_factor);
}

void MapCanvasWidget::zoomOut() {
    fit_pending = false;
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
    // Tool 0 (Select/Move) never paints.
    if (active_tool == 0) return;
    if (active_tool == 2) {
        if (!selecting_entity) return;
        Json& map = project->maps[current_map];
        if (!map.contains("entities") || !map["entities"].is_array()) map["entities"] = JsonArray{};
        Json& entities = map["entities"];
        if (selected_entity_index < 0 ||
            selected_entity_index >= static_cast<int>(editor::kEntityKinds.size())) return;
        const char* kind = editor::kEntityKinds[static_cast<size_t>(selected_entity_index)];
        auto& entity_list = entities.as_array();
        beginStroke(-1, selected_entity_instance >= 0
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
        endStroke();
        update();
        return;
    }
    if (current_layer < 0 || current_layer > 2) return;
    Json& map = project->maps[current_map];
    if (tile.x() >= map["w"].as_int() || tile.y() >= map["h"].as_int() ||
        !map.contains("layers") || !map["layers"].is_object()) return;
    const char *layer_name = editor::mapLayerName(current_layer);
    if (!layer_name || !map["layers"].contains(layer_name) ||
        !map["layers"][layer_name].is_array()) return;
    Json& layer = map["layers"][layer_name];
    auto& rows = layer.as_array();
    if (tile.y() >= static_cast<int>(rows.size()) || !rows[static_cast<size_t>(tile.y())].is_array()) return;
    auto& row = rows[static_cast<size_t>(tile.y())].as_array();
    if (tile.x() >= static_cast<int>(row.size())) return;
    if (active_tool == 3) {
        // Erase Tile: the cell becomes empty.
        if (row[static_cast<size_t>(tile.x())].as_int() < 0) return;
        undo_recorder.noteCell(layer, tile.y(), tile.x());
        row[static_cast<size_t>(tile.x())] = -1;
        update();
        return;
    }
    if (row[static_cast<size_t>(tile.x())].as_int() == selected_tile) return;
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
                undo_recorder.noteCell(layer, y, x);
                stamp_row[static_cast<size_t>(x)] =
                    selected_stamp[static_cast<size_t>(dy * side + dx)];
            }
        }
    } else {
        undo_recorder.noteCell(layer, tile.y(), tile.x());
        row[static_cast<size_t>(tile.x())] = selected_tile;
    }
    update();
}

Json* MapCanvasWidget::editableLayer(int layer_index) {
    if (!project || current_map.empty() || layer_index < 0 || layer_index > 2 ||
        !project->maps.contains(current_map)) return nullptr;
    const char *layer = editor::mapLayerName(layer_index);
    if (!layer) return nullptr;
    Json& layers = project->maps[current_map]["layers"];
    return layers.contains(layer) ? &layers[layer] : nullptr;
}

// Opens a stroke on a map layer (-1 is the entities array).
void MapCanvasWidget::beginStroke(int layer_index, const QString& label) {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    Json& map = project->maps[current_map];
    if (layer_index < 0) {
        if (!map.contains("entities") || !map["entities"].is_array())
            map["entities"] = JsonArray{};
        undo_recorder.beginEntities(map["entities"], label);
        return;
    }
    if (!editableLayer(layer_index)) return;
    undo_recorder.beginLayer(layer_index, label);
}

// Closes the stroke and pushes it as one command.
void MapCanvasWidget::endStroke() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        undo_recorder.cancel();
        return;
    }
    undo_recorder.commit(&project->maps[current_map], undo_stack.get());
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
                beginStroke(-1, QStringLiteral("move entity"));
                stroke_active = true; // endStroke fires at release
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
        // Arm the stroke for the tile tools; the entity tool takes its
        // snapshot where it writes.
        const bool tile_tool = active_tool == 1 || active_tool == 3;
        if (layer && tile.x() >= 0 && tile.y() >= 0 && tile_tool &&
            tile.y() < static_cast<int>(layer->as_array().size()) &&
            tile.x() < static_cast<int>((*layer)[static_cast<size_t>(tile.y())].as_array().size())) {
            beginStroke(current_layer, active_tool == 3 ? QStringLiteral("erase tiles")
                                                       : QStringLiteral("paint tiles"));
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
        if (stroke_active) endStroke();
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
    fit_pending = false;
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
    requestPendingFit();
    update();
}

void MapCanvasWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    requestPendingFit();
}

void MapCanvasWidget::leaveEvent(QEvent* event) {
    if (hover_x >= 0 || hover_y >= 0) {
        hover_x = -1;
        hover_y = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace aur
