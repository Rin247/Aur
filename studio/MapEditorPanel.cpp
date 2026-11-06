#include "MapEditorPanel.hpp"
#include "TileTexture.hpp"
#include <QKeySequence>
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
}

void MapCanvasWidget::setProject(Project* p) {
    project = p;
    undo_layers.clear();
    redo_layers.clear();
    tile_cache.clear();
    update();
}

void MapCanvasWidget::setMap(const std::string& mid) {
    if (current_map != mid) {
        undo_layers.clear();
        redo_layers.clear();
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
    selecting_entity = active_tool == 2;
}

void MapCanvasWidget::setSelectedEntityKind(int entity_kind) {
    selected_entity_index = entity_kind;
    selecting_entity = active_tool == 2;
}

void MapCanvasWidget::setTool(int tool_index) {
    active_tool = std::clamp(tool_index, 0, 3);
    selecting_entity = active_tool == 2;
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
        undo_layers.emplace_back(current_layer, map["layers"][layer]);
        if (undo_layers.size() > 100) undo_layers.erase(undo_layers.begin());
        row[static_cast<size_t>(tile.x())] = -1;
        redo_layers.clear();
        update();
        return;
    }
    if (selecting_entity && active_tool == 2) {
        Json& map = project->maps[current_map];
        if (!map.contains("entities") || !map["entities"].is_array()) map["entities"] = JsonArray{};
        Json& entities = map["entities"];
        static const std::array<const char*, 8> kinds{
            "spawn", "exit", "npc", "chest", "sign", "trigger", "light", "enemy"};
        if (selected_entity_index < 0 || selected_entity_index >= static_cast<int>(kinds.size())) return;
        const char* kind = kinds[static_cast<size_t>(selected_entity_index)];
        if (selected_entity_index == 2 || selected_entity_index == 3) {
            auto& entity_list = entities.as_array();
            const auto selected = std::find_if(entity_list.begin(), entity_list.end(), [kind](const Json& entity) {
                return entity.contains("type") && entity["type"].is_string() &&
                       entity["type"].as_string() == kind;
            });
            if (selected == entity_list.end()) return;
            undo_layers.emplace_back(-1, entities);
            (*selected)["x"] = tile.x();
            (*selected)["y"] = tile.y();
            if (undo_layers.size() > 100) undo_layers.erase(undo_layers.begin());
            redo_layers.clear();
            update();
            return;
        }
        auto& entity_list = entities.as_array();
        auto existing = std::find_if(entity_list.begin(), entity_list.end(), [kind](const Json& entity) {
            return entity.contains("type") && entity["type"].is_string() &&
                   entity["type"].as_string() == kind;
        });
        undo_layers.emplace_back(-1, entities);
        if (existing != entity_list.end()) {
            (*existing)["x"] = tile.x();
            (*existing)["y"] = tile.y();
        } else {
            Json entity = JsonObject{{"id", kind}, {"type", kind},
                                    {"x", tile.x()}, {"y", tile.y()}};
            if (std::string(kind) == "spawn") entity["dir"] = "down";
            entities.as_array().push_back(std::move(entity));
        }

        if (undo_layers.size() > 100) undo_layers.erase(undo_layers.begin());
        redo_layers.clear();
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
    row[static_cast<size_t>(tile.x())] = selected_tile;
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

void MapCanvasWidget::undo() {
    if (undo_layers.empty() || current_map.empty() || !project ||
        !project->maps.contains(current_map)) return;
    const auto [layer_index, snapshot] = undo_layers.back();
    Json* target = layer_index == -1
        ? &project->maps[current_map]["entities"] : editableLayer(layer_index);
    if (!target) return;
    redo_layers.emplace_back(layer_index, *target);
    *target = snapshot;
    undo_layers.pop_back();
    tile_cache.clear();
    update();
}

void MapCanvasWidget::redo() {
    if (redo_layers.empty() || current_map.empty() || !project ||
        !project->maps.contains(current_map)) return;
    const auto [layer_index, snapshot] = redo_layers.back();
    Json* target = layer_index == -1
        ? &project->maps[current_map]["entities"] : editableLayer(layer_index);
    if (!target) return;
    undo_layers.emplace_back(layer_index, *target);
    *target = snapshot;
    redo_layers.pop_back();
    tile_cache.clear();
    update();
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
                if (flags & SOLID) color = QColor(229, 57, 53, 105);
                else if (flags & WATER) color = QColor(30, 136, 229, 115);
                else if (flags & HURT) color = QColor(239, 83, 80, 120);
                else if (flags & SLOW) color = QColor(255, 202, 40, 100);
                else if (flags & ENCOUNTER) color = QColor(171, 71, 188, 95);
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
            const bool selected = selecting_entity && selected_entity_index >= 0 &&
                selected_entity_index < 8 && type == std::array<const char*, 8>{
                    "spawn", "exit", "npc", "chest", "sign", "trigger", "light", "enemy"}
                    [static_cast<size_t>(selected_entity_index)];
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
        Json* layer = editableLayer(current_layer);
        if (layer && tile.x() >= 0 && tile.y() >= 0 &&
            tile.y() < static_cast<int>(layer->as_array().size()) &&
            tile.x() < static_cast<int>((*layer)[static_cast<size_t>(tile.y())].as_array().size()) &&
            (*layer)[static_cast<size_t>(tile.y())][static_cast<size_t>(tile.x())].as_int() != selected_tile) {
            undo_layers.emplace_back(current_layer, *layer);
            if (undo_layers.size() > 100) undo_layers.erase(undo_layers.begin());
            redo_layers.clear();
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
    if (stroke_active && event->buttons().testFlag(Qt::LeftButton)) paintTile(tile);
}

void MapCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) stroke_active = false;
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
    leftLayout->addWidget(new QLabel("Tile / Entity Palette:", this));

    entityTypeWidget = new QListWidget(this);
    entityTypeWidget->setObjectName("mapAssetPalette");
    const std::array<std::pair<const char*, const char*>, 8> entity_kinds{{
        {"spawn", "Spawn Point"}, {"exit", "Exit Door"}, {"npc", "NPC / Citizen"},
        {"chest", "Treasure Chest"}, {"sign", "Sign"}, {"trigger", "Trigger"},
        {"light", "Light"}, {"enemy", "Enemy"}}};
    entityTypeWidget->addItem("[Tile 0] Grass");
    entityTypeWidget->addItem("[Tile 1] Path");
    entityTypeWidget->addItem("[Tile 2] Water");
    for (const auto& [id, label] : entity_kinds) {
        auto* item = new QListWidgetItem(QString("[Entity] %1").arg(label), entityTypeWidget);
        item->setData(Qt::UserRole, QString::fromLatin1(id));
    }
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

    QWidget* inspectorDock = new QWidget(this);
    QFormLayout* form = new QFormLayout(inspectorDock);
    form->addRow("Selected Object:", new QLabel("Tile / Entity", inspectorDock));
    form->addRow("Facing Dir:", new QLineEdit("down", inspectorDock));
    form->addRow("Event ID:", new QLineEdit("talk_elder", inspectorDock));
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
        if (row <= 2) {
            canvas->setSelectedTile(row);
        } else {
            static const std::array<const char*, 8> kinds{
                "spawn", "exit", "npc", "chest", "sign", "trigger", "light", "enemy"};
            const QString type = entityTypeWidget->item(row)->data(Qt::UserRole).toString();
            const auto found = std::find_if(kinds.begin(), kinds.end(), [&type](const char* kind) {
                return type == QLatin1String(kind);
            });
            if (found != kinds.end()) {
                const int entity_kind = static_cast<int>(std::distance(kinds.begin(), found));
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
    }
}

} // namespace aur
