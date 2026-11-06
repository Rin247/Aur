#include "WorldWeatherOverlay.hpp"
#include "WorldMapHelpers.hpp"
#include "aur/world_topology.hpp"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aur::world {

void draw_weather_overlay(WeatherOverlayContext& ctx) {


        const int stride = std::max(8, static_cast<int>(std::round(72.0 / std::max(1.0, ctx.cell))));
        const QRectF weather_bounds = ctx.wraps_x
            ? QRectF(0.0, ctx.bounds.top(), ctx.visible.width(), ctx.bounds.height()) : ctx.bounds;
        const JsonArray* temperature_rows = ctx.temperature_rows;
        ctx.painter.save();
        ctx.painter.setClipRect(weather_bounds.intersected(QRectF(ctx.visible)), Qt::IntersectClip);
        if (ctx.weather == "cloudy") {
            ctx.painter.fillRect(weather_bounds, QColor(53, 65, 75, 28));
        } else if (ctx.weather == "rain") {
            ctx.painter.fillRect(weather_bounds, QColor(50, 87, 108, 34));
        } else if (ctx.weather == "snow") {
            ctx.painter.fillRect(weather_bounds, QColor(203, 222, 234, 38));
        } else if (ctx.weather == "storm") {
            ctx.painter.fillRect(weather_bounds, QColor(24, 34, 47, 76));
        } else if (ctx.weather == "fog") {
            ctx.painter.fillRect(weather_bounds, QColor(199, 215, 223, 72));
        }
        if (ctx.weather == "storm" && ctx.simulation_running &&
            ctx.simulation_tick % 90 < 3) {
            ctx.painter.fillRect(weather_bounds, QColor(220, 232, 240, 68));
        }
        const int first_weather_copy = ctx.wraps_x
            ? static_cast<int>(std::floor(static_cast<double>(ctx.first_x) / ctx.map_width)) : 0;
        const int last_weather_copy = ctx.wraps_x
            ? static_cast<int>(std::ceil(static_cast<double>(ctx.last_x) / ctx.map_width)) : 1;
        for (int y = stride / 2; y < ctx.map_height; y += stride) {
            for (int copy = first_weather_copy; copy < last_weather_copy; ++copy) {
            for (int local_x = stride / 2; local_x < ctx.map_width; local_x += stride) {
                const int world_x = copy * ctx.map_width + local_x;
                if (world_x < ctx.first_x || world_x >= ctx.last_x) continue;
                const int x = ctx.wraps_x ? aur::wrapX(world_x, ctx.map_width) : world_x;
                if (ctx.weather == "clear" || ctx.weather == "fog") continue;
                const uint32_t simulation_step = static_cast<uint32_t>(
                    ctx.simulation_running ? ctx.simulation_tick / 100 : 0);
                const uint32_t hash = static_cast<uint32_t>(ctx.worldgen_seed) ^
                    static_cast<uint32_t>(x) * 73856093u ^
                    static_cast<uint32_t>(y) * 19349663u ^
                    simulation_step * 83492791u ^
                    static_cast<uint32_t>(std::floor(ctx.hour / 3.0)) * 2654435761u;
                const uint32_t mixed = (hash ^ (hash >> 13)) * 1274126177u;
                const uint32_t weather_roll = mixed ^ (mixed >> 16);
                const bool ocean = ctx.is_ocean_at(x, y);
                std::string climate;
                const JsonArray* biome_rows = ctx.biome_rows;
                if (biome_rows && y < static_cast<int>(biome_rows->size()) &&
                    (*biome_rows)[static_cast<size_t>(y)].is_array() &&
                    x < static_cast<int>((*biome_rows)[static_cast<size_t>(y)].as_array().size())) {
                    const std::string biome_id = (*biome_rows)[static_cast<size_t>(y)]
                        [static_cast<size_t>(x)].as_string();
                    const auto biome = ctx.project->biomes.find(biome_id);
                    if (biome != ctx.project->biomes.end() && biome->second["climate"].is_string()) {
                        climate = biome->second["climate"].as_string();
                    }
                }
                int temperature_value = 50;
                if (temperature_rows && y < static_cast<int>(temperature_rows->size()) &&
                    (*temperature_rows)[static_cast<size_t>(y)].is_array() &&
                    x < static_cast<int>((*temperature_rows)[static_cast<size_t>(y)].as_array().size())) {
                    temperature_value = (*temperature_rows)[static_cast<size_t>(y)]
                        [static_cast<size_t>(x)].as_int();
                }
                const std::string weather = ctx.weather == "climate"
                    ? world::climate_weather(ctx.project, climate, weather_roll) : ctx.weather;
                if (weather == "clear") continue;
                const uint32_t position_hash = static_cast<uint32_t>(ctx.worldgen_seed) ^
                    static_cast<uint32_t>(x) * 73856093u ^
                    static_cast<uint32_t>(y) * 19349663u;
                const uint32_t position_mixed =
                    (position_hash ^ (position_hash >> 13)) * 1274126177u;
                const uint32_t position_roll = position_mixed ^ (position_mixed >> 16);
                const uint32_t coverage = weather == "storm" ? 92u
                    : weather == "cloudy" ? 86u : 72u;
                if (position_roll % 100u >= coverage) continue;
                const bool snow = weather == "snow" ||
                    ((weather == "rain" || weather == "storm") &&
                     (climate == "arctic" ||
                      (temperature_value >= 0 && temperature_value < 30)));
                const bool precipitation =
                    weather == "rain" || weather == "snow" || weather == "storm";
                const qreal jitter_scale = stride * ctx.cell * 0.28;
                const qreal jitter_x =
                    (static_cast<qreal>(position_roll & 0xffffu) / 65535.0 - 0.5) *
                    jitter_scale;
                const qreal jitter_y =
                    (static_cast<qreal>(position_roll >> 16) / 65535.0 - 0.5) *
                    jitter_scale;
                const QPointF center(ctx.bounds.left() + (world_x + 0.5) * ctx.cell + jitter_x,
                                     ctx.bounds.top() + (y + 0.5) * ctx.cell + jitter_y);
                const qreal size_scale = 0.78 +
                    static_cast<qreal>((position_roll >> 8) & 0xffu) / 255.0 * 0.42;
                const qreal size = std::clamp(ctx.cell * 1.8 * size_scale, 12.0, 30.0);
                const qreal drift = ctx.simulation_running
                    ? std::sin((ctx.simulation_tick + x * 5 + y * 3) * 0.04) * size * 0.25 : 0.0;
                const QPointF cloud_center = center + QPointF(drift, 0.0);
                QColor cloud_color = weather == "storm" ? QColor("#53616c")
                    : snow ? QColor("#cfe5ef") : QColor("#d3d9d5");
                cloud_color.setAlpha(145 + static_cast<int>((position_roll >> 24) % 90u));
                QColor cloud_outline = weather == "storm" ? QColor("#26323c")
                                                           : QColor("#263746");
                cloud_outline.setAlpha(145 + static_cast<int>((position_roll >> 24) % 90u));
                ctx.painter.setPen(QPen(cloud_outline,
                                    std::max<qreal>(1.0, ctx.cell * 0.06)));
                ctx.painter.setBrush(cloud_color);
                ctx.painter.drawEllipse(QRectF(cloud_center.x() - size * 0.28,
                    cloud_center.y() - size * 0.02, size * 0.42, size * 0.32));
                ctx.painter.drawEllipse(QRectF(cloud_center.x() - size * 0.08,
                    cloud_center.y() - size * 0.20, size * 0.48, size * 0.50));
                ctx.painter.drawEllipse(QRectF(cloud_center.x() + size * 0.16,
                    cloud_center.y() - size * 0.02, size * 0.40, size * 0.32));
                if (precipitation && ctx.weather == "climate") {
                    ctx.painter.setPen(QPen(snow ? QColor("#effaff") : QColor("#6dc5e6"),
                                        std::max<qreal>(1.0, ctx.cell * 0.07), Qt::SolidLine,
                                        Qt::RoundCap));
                    const qreal phase = ctx.simulation_running
                        ? std::fmod(ctx.simulation_tick * 0.45 + x * 3 + y, size) : size * 0.45;
                    for (int drop = -1; drop <= 1; ++drop) {
                        const qreal drop_x = cloud_center.x() + drop * size * 0.25;
                        const qreal drop_y = cloud_center.y() + size * 0.22 + phase * 0.15;
                        ctx.painter.drawLine(QPointF(drop_x, drop_y),
                                         QPointF(drop_x - size * 0.08, drop_y + size * 0.24));
                    }
                }
                }
            }
        }
        if (ctx.weather == "rain" || ctx.weather == "snow" ||
            ctx.weather == "storm") {
            const QRectF visible_bounds = weather_bounds.intersected(QRectF(ctx.visible));
            constexpr int spacing = 40;
            const int phase = ctx.simulation_running
                ? ctx.simulation_tick % spacing : spacing / 2;
            const bool snow = ctx.weather == "snow";
            ctx.painter.setPen(QPen(
                snow ? QColor(239, 249, 255, 210)
                     : ctx.weather == "storm" ? QColor(92, 175, 216, 190)
                                                  : QColor(105, 187, 224, 175),
                snow ? 1.0 : 1.3, Qt::SolidLine, Qt::RoundCap));
            const qreal start_y = visible_bounds.top() +
                std::fmod(phase + spacing, spacing);
            for (qreal y = start_y; y < visible_bounds.bottom(); y += spacing) {
                const int row = static_cast<int>(
                    std::floor((y - visible_bounds.top()) / spacing));
                const qreal row_offset = (row & 1) != 0 ? spacing * 0.5 : 0.0;
                for (qreal x = visible_bounds.left() + row_offset;
                     x < visible_bounds.right(); x += spacing) {
                    const int column = static_cast<int>(std::floor(
                        (x - visible_bounds.left() - row_offset) / spacing));
                    const uint32_t particle_hash = static_cast<uint32_t>(ctx.worldgen_seed) ^
                        static_cast<uint32_t>(row) * 73856093u ^
                        static_cast<uint32_t>(column) * 19349663u;
                    const uint32_t particle_mixed =
                        (particle_hash ^ (particle_hash >> 13)) * 1274126177u;
                    const uint32_t particle_roll = particle_mixed ^ (particle_mixed >> 16);
                    const qreal jitter_x =
                        (static_cast<qreal>(particle_roll & 0xffu) / 255.0 - 0.5) *
                        spacing * 0.5;
                    const qreal jitter_y =
                        (static_cast<qreal>((particle_roll >> 8) & 0xffu) / 255.0 - 0.5) *
                        spacing * 0.35;
                    const qreal particle_x = x + jitter_x;
                    const qreal particle_y = y + jitter_y;
                    if (snow) {
                        const qreal drift = std::sin(
                            (ctx.simulation_tick + static_cast<int>(particle_x * 3 + particle_y)) *
                            0.025) * 3.0;
                        ctx.painter.drawLine(QPointF(particle_x + drift - 2.0, particle_y),
                                         QPointF(particle_x + drift + 2.0, particle_y));
                        ctx.painter.drawLine(QPointF(particle_x + drift, particle_y - 2.0),
                                         QPointF(particle_x + drift, particle_y + 2.0));
                    } else {
                        ctx.painter.drawLine(QPointF(particle_x, particle_y),
                                         QPointF(particle_x - 4.0, particle_y + 10.0));
                    }
                }
            }
        }
        ctx.painter.restore();
    


    ctx.painter.restore();
}

}
