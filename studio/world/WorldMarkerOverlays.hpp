#pragma once

#include "aur/json.hpp"
#include "aur/project.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>

namespace aur::world {

struct EntityMarkersContext {
    QPainter& painter;
    const Project* project = nullptr;
    const JsonArray* entities = nullptr;
    QRectF bounds;
    QRectF visible;
    qreal cell = 0.0;
    qreal zoom = 1.0;
    int map_width = 0;
    int map_height = 0;
    int selected_node = -1;
    bool wraps_x = false;
};

void draw_entity_markers(EntityMarkersContext& context);

struct HoverContext {
    QPainter& painter;
    QRectF bounds;
    qreal cell = 0.0;
    int hover_x = -1;
    int hover_y = -1;
    int map_width = 0;
    int map_height = 0;
    qreal viewport_width = 0.0;
    bool wraps_x = false;
};

void draw_hover_highlight(HoverContext& context);

struct ZoomLabelContext {
    QPainter& painter;
    bool hd_surface = false;
    int map_width = 0;
    int map_height = 0;
    std::string tileset;
    qreal zoom = 1.0;
    qreal viewport_width = 0.0;
};

void draw_zoom_label(ZoomLabelContext& context);

}
