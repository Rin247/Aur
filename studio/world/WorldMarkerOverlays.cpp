#include "WorldMarkerOverlays.hpp"
#include "aur/world_topology.hpp"

#include <QFontMetrics>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace aur::world
{

    void draw_entity_markers(EntityMarkersContext &ctx)
    {
        if (!ctx.entities)
            return;
        const auto &entities = *ctx.entities;
        for (size_t index = 0; index < entities.size(); ++index)
        {
            const Json &entity = entities[index];
            int x = entity["x"].as_int();
            const int y = entity["y"].as_int();
            if (y < 0 || y >= ctx.map_height ||
                (!ctx.wraps_x && (x < 0 || x >= ctx.map_width)))
                continue;
            if (ctx.wraps_x)
                x = aur::wrapX(x, ctx.map_width);
            const qreal marker_x = ctx.bounds.left() + (x + 0.5) * ctx.cell;
            const int first_marker_copy = ctx.wraps_x
                                              ? static_cast<int>(std::floor((0.0 - marker_x) / ctx.bounds.width()))
                                              : 0;
            const int last_marker_copy = ctx.wraps_x
                                             ? static_cast<int>(std::ceil((ctx.visible.width() - marker_x) /
                                                                          ctx.bounds.width()))
                                             : 1;
            const bool is_exit = entity.contains("type") && entity["type"].as_string() == "exit";
            const QColor marker = is_exit ? QColor("#d596ff") : QColor("#ffd27a");
            const qreal radius = std::clamp(ctx.cell * 0.32, 4.0, 8.0);
            for (int copy = first_marker_copy; copy < last_marker_copy; ++copy)
            {
                const QPointF center(marker_x + copy * ctx.bounds.width(),
                                     ctx.bounds.top() + (y + 0.5) * ctx.cell);
                ctx.painter.setPen(QPen(index == static_cast<size_t>(ctx.selected_node)
                                            ? Qt::white
                                            : QColor("#202329"),
                                        index == static_cast<size_t>(ctx.selected_node) ? 2.0 : 1.0));
                ctx.painter.setBrush(marker);
                if (is_exit)
                {
                    QPolygonF diamond;
                    diamond << QPointF(center.x(), center.y() - radius)
                            << QPointF(center.x() + radius, center.y())
                            << QPointF(center.x(), center.y() + radius)
                            << QPointF(center.x() - radius, center.y());
                    ctx.painter.drawPolygon(diamond);
                }
                else
                {
                    ctx.painter.drawEllipse(center, radius, radius);
                }

                QString label;
                if (is_exit && entity.contains("target") && entity["target"].contains("map"))
                {
                    label = QString::fromStdString(entity["target"]["map"].as_string());
                }
                else if (entity.contains("id"))
                {
                    label = QString::fromStdString(entity["id"].as_string());
                }
                if (!label.isEmpty() && ctx.zoom >= 0.75)
                {
                    const QFontMetrics metrics(ctx.painter.font());
                    const QRect textRect = metrics.boundingRect(label).adjusted(-4, -2, 4, 2);
                    const QPoint labelOrigin(static_cast<int>(center.x() + radius + 3),
                                             static_cast<int>(center.y() - textRect.height() / 2));
                    ctx.painter.fillRect(QRect(labelOrigin, textRect.size()), QColor(14, 17, 21, 210));
                    ctx.painter.setPen(QColor("#f2f4f6"));
                    ctx.painter.drawText(labelOrigin.x() + 4, labelOrigin.y() + metrics.ascent(), label);
                }
            }
        }
    }

    void draw_hover_highlight(HoverContext &ctx)
    {
        if (ctx.hover_x < 0 || ctx.hover_x >= ctx.map_width ||
            ctx.hover_y < 0 || ctx.hover_y >= ctx.map_height)
            return;
        qreal hover_x = ctx.bounds.left() + ctx.hover_x * ctx.cell;
        if (ctx.wraps_x)
        {
            hover_x += std::round((ctx.viewport_width * 0.5 - hover_x) /
                                  ctx.bounds.width()) *
                       ctx.bounds.width();
        }
        const QRectF hover(hover_x, ctx.bounds.top() + ctx.hover_y * ctx.cell,
                           ctx.cell, ctx.cell);
        ctx.painter.setPen(QPen(QColor("#ffffff"), 1.5));
        ctx.painter.setBrush(Qt::NoBrush);
        ctx.painter.drawRect(hover);
    }

    void draw_zoom_label(ZoomLabelContext &ctx)
    {
        ctx.painter.setPen(QColor("#c1c8ce"));
        const QString zoomText = ctx.hd_surface
                                     ? QString(QStringLiteral("%1 \u00d7 %2 tiles  \u00b7  %3  \u00b7  1 art px = %4 screen px"))
                                           .arg(ctx.map_width)
                                           .arg(ctx.map_height)
                                           .arg(QString::fromStdString(ctx.tileset))
                                           .arg(QString::number(ctx.zoom, 'g', 3))
                                     : QString(QStringLiteral("%1 \u00d7 %2 tiles  \u00b7  %3  \u00b7  %4%"))
                                           .arg(ctx.map_width)
                                           .arg(ctx.map_height)
                                           .arg(QString::fromStdString(ctx.tileset))
                                           .arg(qRound(ctx.zoom * 100.0));
        ctx.painter.drawText(QRectF(16, 12, ctx.viewport_width - 32, 20),
                             Qt::AlignLeft | Qt::AlignVCenter, zoomText);
    }

}
