#pragma once

#include "world/WorldMapTools.hpp"
#include "aur/world_topology.hpp"

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <algorithm>
#include <cmath>

namespace aur::studio
{

    struct WorldCameraState
    {
        qreal zoom = 1.0;
        QPointF pan;
    };

    struct WorldCameraSpec
    {
        int map_width = 0;
        int map_height = 0;
        qreal source_tile_size = 1.0;
        bool wraps_x = false;
        // WP1: the editor view shows the wrapped world ONCE by default (owner
        // decision recorded in the work-packages brief); when true the camera
        // keeps the old unbounded repeating-X behaviour.
        bool repeat_view_x = false;
        qreal zoom = 1.0;
        QPointF pan;
        QSizeF viewport;
    };

    class WorldCamera
    {
    public:
        static qreal clampZoom(qreal zoom) { return std::clamp(zoom, 0.05, 16.0); }

        static QRectF mapBounds(const WorldCameraSpec &spec, const QSizeF &widget)
        {
            if (spec.map_width <= 0 || spec.map_height <= 0)
                return {};
            const QSizeF size(spec.map_width * spec.source_tile_size * spec.zoom,
                              spec.map_height * spec.source_tile_size * spec.zoom);
            return QRectF((widget.width() - size.width()) / 2.0 + spec.pan.x(),
                          (widget.height() - size.height()) / 2.0 + spec.pan.y(),
                          size.width(), size.height());
        }

        static void constrain(WorldCameraState &state, const WorldCameraSpec &spec)
        {
            const QSizeF widget = spec.viewport;
            WorldCameraSpec bounds_spec = spec;
            bounds_spec.pan = state.pan;
            const QRectF bounds = mapBounds(bounds_spec, widget);
            if (bounds.isEmpty())
                return;
            // WP1: repeat_view_x keeps the legacy unbounded horizontal pan;
            // otherwise both axes clamp to the map with a small margin so a
            // smaller-than-viewport map stays centred.
            if (spec.wraps_x && spec.repeat_view_x)
            {
                const qreal vertical_pan_limit =
                    std::max<qreal>(0.0, (bounds.height() - widget.height()) * 0.5);
                state.pan.setY(std::clamp(state.pan.y(), -vertical_pan_limit, vertical_pan_limit));
                if (bounds.width() > 0.0)
                    state.pan.setX(std::remainder(state.pan.x(), bounds.width()));
                return;
            }
            constexpr qreal margin = 24.0;
            const qreal horizontal_pan_limit =
                std::max<qreal>(0.0, (bounds.width() - widget.width()) * 0.5 + margin);
            const qreal vertical_pan_limit =
                std::max<qreal>(0.0, (bounds.height() - widget.height()) * 0.5 + margin);
            state.pan.setX(std::clamp(state.pan.x(), -horizontal_pan_limit, horizontal_pan_limit));
            state.pan.setY(std::clamp(state.pan.y(), -vertical_pan_limit, vertical_pan_limit));
        }

        static QPoint tileAt(const WorldCameraSpec &spec, const QPointF &position)
        {
            const QSizeF widget = spec.viewport;
            WorldCameraSpec bounds_spec = spec;
            bounds_spec.pan = spec.pan;
            const QRectF bounds = mapBounds(bounds_spec, widget);
            if (position.y() < bounds.top() || position.y() >= bounds.bottom())
                return {-1, -1};
            const qreal cell = bounds.width() / spec.map_width;
            const int tile_x = static_cast<int>(std::floor((position.x() - bounds.left()) / cell));
            const int tile_y = static_cast<int>(std::floor((position.y() - bounds.top()) / cell));
            if (tile_y < 0 || tile_y >= spec.map_height)
                return {-1, -1};
            if (spec.wraps_x && spec.repeat_view_x)
                return {aur::wrapX(tile_x, spec.map_width), tile_y};
            if (tile_x < 0 || tile_x >= spec.map_width)
                return {-1, -1};
            return {tile_x, tile_y};
        }

        static qreal fitZoom(const WorldCameraSpec &spec, const QSizeF &widget)
        {
            if (spec.map_width <= 0 || spec.map_height <= 0)
                return 1.0;
            constexpr qreal padding = 24.0;
            return std::min((widget.width() - padding * 2) /
                                (spec.map_width * spec.source_tile_size),
                            (widget.height() - padding * 2) /
                                (spec.map_height * spec.source_tile_size));
        }
    };

}
