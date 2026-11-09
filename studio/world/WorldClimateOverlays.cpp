#include "WorldClimateOverlays.hpp"
#include "aur/clock.hpp"
#include "aur/game.hpp"
#include "aur/world_topology.hpp"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <limits>

namespace aur::world {

void draw_temperature_overlay(TemperatureOverlayContext& ctx) {

        const auto& rows = *ctx.temperature_rows;
        const QColor cold("#548ed8");
        const QColor hot("#ee7652");
        for (int y = ctx.first_y; y < ctx.last_y && y < static_cast<int>(rows.size()); ++y) {
            const Json& row = rows[static_cast<size_t>(y)];
            if (!row.is_array()) continue;
            for (int x = ctx.first_x; x < ctx.last_x; ++x) {
                const int source_x = ctx.wraps_x ? aur::wrapX(x, ctx.map_width) : x;
                if (source_x < 0 || source_x >= static_cast<int>(row.as_array().size())) continue;
                const int value = row[static_cast<size_t>(source_x)].as_int();
                if (value < 0) continue;
                const qreal mix = std::clamp(value / 100.0, 0.0, 1.0);
                QColor color(
                    qRound(cold.red() + (hot.red() - cold.red()) * mix),
                    qRound(cold.green() + (hot.green() - cold.green()) * mix),
                    qRound(cold.blue() + (hot.blue() - cold.blue()) * mix), 92);
                ctx.painter.fillRect(QRectF(ctx.bounds.left() + x * ctx.cell, ctx.bounds.top() + y * ctx.cell,
                                        ctx.cell, ctx.cell), color);
            }
        }


}

void draw_climate_axes(ClimateAxesContext& ctx) {

        ctx.painter.save();
        ctx.painter.setPen(QPen(QColor("#f6e5ad"), std::max<qreal>(1.5, ctx.cell * 0.07),
                            Qt::DashLine));
        const qreal marker_radius = std::clamp(ctx.cell * 0.32, 4.0, 9.0);
        if (ctx.wraps_x) {
            ctx.painter.setClipRect(QRectF(0.0, ctx.bounds.top(), ctx.visible.width(), ctx.bounds.height()),
                                Qt::IntersectClip);
            const qreal equator_y = ctx.bounds.top() + ctx.bounds.height() * 0.5;
            ctx.painter.drawLine(QPointF(0.0, equator_y), QPointF(ctx.visible.width(), equator_y));
            ctx.painter.setPen(QPen(QColor("#172432"), 1.5));
            ctx.painter.setBrush(QColor("#70b8ff"));
            ctx.painter.drawEllipse(QPointF(12.0, ctx.bounds.top() + marker_radius),
                                marker_radius, marker_radius);
            ctx.painter.setBrush(QColor("#ff9a67"));
            ctx.painter.drawEllipse(QPointF(12.0, ctx.bounds.bottom() - marker_radius),
                                marker_radius, marker_radius);
            ctx.painter.setPen(QColor("#f6f2e8"));
            ctx.painter.drawText(QPointF(24.0, ctx.bounds.top() + marker_radius + 4), "N");
            ctx.painter.drawText(QPointF(24.0, ctx.bounds.bottom() - marker_radius + 4), "S");
        } else {
            const Json& map = *ctx.map;
    const int angle_degrees = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt")
        ? map["worldgen"]["axial_tilt"].as_int() : 0;
            const qreal angle = angle_degrees * 3.14159265358979323846 / 180.0;
            const QPointF center = ctx.bounds.center();
            const qreal direction_x = std::cos(angle);
            const qreal direction_y = -std::sin(angle);
            const qreal normal_x = std::sin(angle);
            const qreal normal_y = std::cos(angle);
            const qreal half_width = ctx.bounds.width() * 0.5;
            const qreal half_height = ctx.bounds.height() * 0.5;
            const qreal equator_extent = std::min(
                std::abs(direction_x) < 1e-6 ? std::numeric_limits<qreal>::max()
                                             : half_width / std::abs(direction_x),
                std::abs(direction_y) < 1e-6 ? std::numeric_limits<qreal>::max()
                                             : half_height / std::abs(direction_y));
            const qreal pole_extent = std::min(
                std::abs(normal_x) < 1e-6 ? std::numeric_limits<qreal>::max()
                                          : half_width / std::abs(normal_x),
                std::abs(normal_y) < 1e-6 ? std::numeric_limits<qreal>::max()
                                          : half_height / std::abs(normal_y));
            ctx.painter.drawLine(center - QPointF(direction_x, direction_y) * equator_extent,
                             center + QPointF(direction_x, direction_y) * equator_extent);
            const QPointF north = center - QPointF(normal_x, normal_y) * pole_extent;
            const QPointF south = center + QPointF(normal_x, normal_y) * pole_extent;
            ctx.painter.setPen(QPen(QColor("#172432"), 1.5));
            ctx.painter.setBrush(QColor("#70b8ff"));
            ctx.painter.drawEllipse(north, marker_radius, marker_radius);
            ctx.painter.setBrush(QColor("#ff9a67"));
            ctx.painter.drawEllipse(south, marker_radius, marker_radius);
            ctx.painter.setPen(QColor("#f6f2e8"));
            ctx.painter.drawText(north + QPointF(marker_radius + 3, 4), "N");
            ctx.painter.drawText(south + QPointF(marker_radius + 3, 4), "S");
        }


}


void draw_flow_overlay(FlowOverlayContext& ctx) {
    if (ctx.cell <= 0) return;
    const Json& map = *ctx.map;
    const int axis_degrees = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt") ? map["worldgen"]["axial_tilt"].as_int() : 0;
    const qreal axis = axis_degrees * 3.14159265358979323846 / 180.0;
    const qreal axis_sine = std::sin(axis);
    const qreal axis_cosine = std::cos(axis);
    const qreal latitude_scale = std::max<qreal>(
        0.001, std::abs(axis_sine) + std::abs(axis_cosine));
    const qreal east_x = ctx.wraps_x ? 1.0 : axis_cosine;
    const qreal east_y = ctx.wraps_x ? 0.0 : -axis_sine;
    const qreal north_x = ctx.wraps_x ? 0.0 : axis_sine;
    const qreal north_y = ctx.wraps_x ? 1.0 : axis_cosine;
    const int stride = std::max(1, static_cast<int>(std::round(28.0 / std::max(1.0, ctx.cell))));
    const auto draw_flow = [&](int x, int y, bool current) {
        const int source_x = ctx.wraps_x ? aur::wrapX(x, ctx.map_width) : x;
        const double nx = (source_x + 0.5) / ctx.map_width * 2.0 - 1.0;
        const double ny = (y + 0.5) / ctx.map_height * 2.0 - 1.0;
        const double latitude = ctx.wraps_x ? ny
            : (nx * axis_sine + ny * axis_cosine) / latitude_scale;
        double vx = 0.0;
        double vy = 0.0;
        if (current) {
            if (ctx.wraps_x) {
                const double longitude = 2.0 * 3.14159265358979323846 *
                    (source_x + 0.5) / ctx.map_width;
                const double gyre = std::sin(latitude * 3.14159265358979323846);
                vx = std::cos(longitude) * gyre;
                vy = -std::sin(longitude) * gyre * 0.35;
            } else {
                const double center_x =
                    (x / static_cast<double>(ctx.map_width) < 0.5) ? 0.28 : 0.72;
                const double center_y = latitude >= 0.0 ? 0.34 : 0.66;
                const double dx = x / static_cast<double>(ctx.map_width) - center_x;
                const double dy = y / static_cast<double>(ctx.map_height) - center_y;
                const double spin = latitude >= 0.0 ? 1.0 : -1.0;
                vx = -dy * spin;
                vy = dx * spin;
            }
        } else {
            const double abs_latitude = std::abs(latitude);
            const double zonal = abs_latitude < 0.28 || abs_latitude > 0.68 ? -1.0 : 1.0;
            const double meridional = latitude == 0.0 ? 0.0 : -std::copysign(0.24, latitude);
            vx = east_x * zonal + north_x * meridional;
            vy = east_y * zonal + north_y * meridional;
        }
        const double magnitude = std::hypot(vx, vy);
        if (magnitude < 0.001) return;
        vx /= magnitude;
        vy /= magnitude;
        const QPointF center(ctx.bounds.left() + (x + 0.5) * ctx.cell,
                             ctx.bounds.top() + (y + 0.5) * ctx.cell);
        const qreal length = std::clamp(ctx.cell * 0.65, 5.0, 15.0);
        const QPointF start = center - QPointF(vx, vy) * (length * 0.5);
        const QPointF end = center + QPointF(vx, vy) * (length * 0.5);
        const QColor flow_color = current ? QColor("#67d4e4") : QColor("#fff0b0");
        ctx.painter.setPen(QPen(flow_color, std::clamp(ctx.cell * 0.09, 1.0, 2.0),
                            Qt::SolidLine, Qt::RoundCap));
        ctx.painter.drawLine(start, end);
        const QPointF side(-vy, vx);
        ctx.painter.drawLine(end, end - QPointF(vx, vy) * length * 0.32 + side * length * 0.22);
        ctx.painter.drawLine(end, end - QPointF(vx, vy) * length * 0.32 - side * length * 0.22);
        if (ctx.simulation_running) {
            const int phase = (ctx.simulation_tick + source_x * 7 + y * 13) % 80;
            const qreal travel = phase / 79.0 - 0.5;
            const QPointF particle = center + QPointF(vx, vy) * (travel * length);
            ctx.painter.setPen(Qt::NoPen);
            ctx.painter.setBrush(flow_color.lighter(145));
            ctx.painter.drawEllipse(particle, std::clamp(ctx.cell * 0.12, 1.2, 2.6),
                                std::clamp(ctx.cell * 0.12, 1.2, 2.6));
        }
    };
    ctx.painter.save();
    const int first_flow_copy = ctx.wraps_x
        ? static_cast<int>(std::floor(static_cast<double>(ctx.first_x) / ctx.map_width)) : 0;
    const int last_flow_copy = ctx.wraps_x
        ? static_cast<int>(std::ceil(static_cast<double>(ctx.last_x) / ctx.map_width)) : 1;
    if (!ctx.currents) {
        for (int y = stride / 2; y < ctx.map_height; y += stride) {
            for (int copy = first_flow_copy; copy < last_flow_copy; ++copy) {
                for (int x = stride / 2; x < ctx.map_width; x += stride) {
                    const int world_x = copy * ctx.map_width + x;
                    if (world_x >= ctx.first_x && world_x < ctx.last_x) draw_flow(world_x, y, false);
                }
            }
        }
    }
    if (ctx.currents) {
        for (int y = stride / 2; y < ctx.map_height; y += stride) {
            for (int copy = first_flow_copy; copy < last_flow_copy; ++copy) {
                for (int x = stride / 2; x < ctx.map_width; x += stride) {
                    const int world_x = copy * ctx.map_width + x;
                    if (world_x >= ctx.first_x && world_x < ctx.last_x &&
                        ctx.is_ocean_at(world_x, y)) {
                        draw_flow(world_x, y, true);
                    }
                }
            }
        }
    }
    ctx.painter.restore();
}

void draw_daynight_fallback(DayNightContext& ctx) {
    GameClock lighting = project_clock(ctx.project);
    lighting.set_time(ctx.hour);
    const auto [rgb, darkness] = lighting.ambient();
    QColor tint(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb));
    tint.setAlphaF(std::clamp(darkness, 0.0, 1.0));
    const QRectF lighting_bounds = ctx.wraps_x
        ? QRectF(0.0, ctx.bounds.top(), ctx.visible.width(), ctx.bounds.height()) : ctx.bounds;
    ctx.painter.fillRect(lighting_bounds, tint);
}

}
