#include "WorldWeatherOverlay.hpp"
#include "WeatherPreviewField.hpp"
#include "WeatherRender.hpp"
#include "WorldMapHelpers.hpp"
#include "aur/world_topology.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aur::world {

// WP3: the overlay is a thin compositor now. The preview WeatherField is
// built (coherent noise or climate-profile mix), rendered to ONE low-res
// image by render_weather_image, and drawn once into the map bounds. Forced
// fog keeps a light full-map wash because the field alone is too subtle.
void draw_weather_overlay(WeatherOverlayContext& ctx) {
    if (ctx.weather.empty() || ctx.weather == "clear") return;
    if (!ctx.project || ctx.cell <= 0.0) return;

    const double time_s = ctx.simulation_running
        ? static_cast<double>(ctx.simulation_tick) / 10.0 : 0.0;
    const std::uint32_t seed = static_cast<std::uint32_t>(
        ctx.worldgen_seed > 0 ? ctx.worldgen_seed : 1);
    const WeatherField field = make_preview_field(
        ctx.project, *ctx.map, ctx.weather, seed, time_s);
    if (!field.valid()) return;

    WeatherRenderParams params;
    params.hour = ctx.hour;
    params.texels_per_tile = ctx.cell >= 8.0 ? 4 : (ctx.cell >= 4.0 ? 2 : 1);
    params.sun_dx = -0.6f;
    params.sun_dy = 0.8f;
    params.shadows = true;
    // Ambient from the hour: dim the weather with the day/night cycle.
    const double day_phase = std::cos((ctx.hour - 12.0) * 3.14159265358979 / 12.0);
    params.ambient_darkness = static_cast<float>(std::clamp(0.5 - 0.5 * day_phase, 0.0, 0.7));

    const QImage image = render_weather_image(field, params);
    if (image.isNull()) return;

    ctx.painter.save();
    ctx.painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    if (ctx.wraps_x) {
        const int first_copy = static_cast<int>(
            std::floor(static_cast<double>(ctx.first_x) / ctx.map_width));
        const int last_copy = static_cast<int>(
            std::ceil(static_cast<double>(ctx.last_x) / ctx.map_width));
        for (int copy = first_copy; copy < last_copy; ++copy) {
            QRectF target = ctx.bounds;
            target.moveLeft(ctx.bounds.left() + copy * ctx.bounds.width());
            if (target.intersects(QRectF(ctx.visible)))
                ctx.painter.drawImage(target, image);
        }
    } else {
        ctx.painter.drawImage(ctx.bounds, image);
    }
    if (ctx.weather == "fog") {
        ctx.painter.fillRect(ctx.bounds.intersected(QRectF(ctx.visible)),
                             QColor(199, 215, 223, 46));
    }
    ctx.painter.restore();
}

} // namespace aur::world
