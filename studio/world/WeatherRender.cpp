#include "WeatherRender.hpp"

#include <QPoint>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aur::world {

namespace {

// Deterministic hash noise (PCG-style mix), X-periodic at the given period so
// the advected fBm lattice never tears at the seam.
std::uint32_t hash_u32(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float hash_float(std::uint32_t hash) {
    return static_cast<float>(hash & 0xffffffu) / 16777216.0f;
}

// Value noise with an X-periodic lattice (period in lattice cells).
float value_noise(float x, float y, int period_x, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const auto cell_hash = [&](int cx, int cy) {
        cx = ((cx % period_x) + period_x) % period_x;
        return hash_float(hash_u32(seed ^
            static_cast<std::uint32_t>(cx) * 374761393u ^
            static_cast<std::uint32_t>(cy) * 668265263u));
    };
    const float smooth_x = fx * fx * (3.0f - 2.0f * fx);
    const float smooth_y = fy * fy * (3.0f - 2.0f * fy);
    const float top = cell_hash(x0, y0) * (1.0f - smooth_x) + cell_hash(x0 + 1, y0) * smooth_x;
    const float bottom = cell_hash(x0, y0 + 1) * (1.0f - smooth_x) + cell_hash(x0 + 1, y0 + 1) * smooth_x;
    return top * (1.0f - smooth_y) + bottom * smooth_y;
}

float fbm(float x, float y, int period_x, std::uint32_t seed) {
    float amplitude = 0.5f;
    float total = 0.0f;
    float frequency = 1.0f;
    for (int octave = 0; octave < 4; ++octave) {
        total += amplitude * value_noise(x * frequency, y * frequency,
                                         std::max(1, period_x * static_cast<int>(frequency)),
                                         seed + static_cast<std::uint32_t>(octave) * 7919u);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return std::clamp(total, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float value) {
    const float t = std::clamp((value - edge0) / std::max(1e-6f, edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Posterize alpha into 5 bands to match the pixel-art look.
int posterize(float coverage) {
    const float bands[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int band = 0; band < 5; ++band) {
        if (coverage < bands[band + 1] || band == 4)
            return band;
    }
    return 4;
}

} // namespace

QImage render_weather_image(const WeatherField& field, const WeatherRenderParams& params) {
    if (!field.valid()) return QImage();
    int texels = std::max(1, params.texels_per_tile);
    // Cap the image at roughly 1 MP.
    while (static_cast<long long>(field.width) * texels *
               static_cast<long long>(field.height) * texels > 1024 * 1024 && texels > 1) {
        --texels;
    }
    const int image_width = field.width * texels;
    const int image_height = field.height * texels;
    QImage image(image_width, image_height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const int lattice_period = field.width * texels;  // X-periodic seam-safe
    const double hour = params.hour;
    const float darkness = std::clamp(params.ambient_darkness, 0.0f, 1.0f);

    for (int py = 0; py < image_height; ++py) {
        for (int px = 0; px < image_width; ++px) {
            // Field sample position (tiles, bilinear over the field layers).
            const float tile_x = static_cast<float>(px) / texels - 0.5f;
            const float tile_y = static_cast<float>(py) / texels - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(tile_x)), 0, field.width - 1);
            const int x1 = std::min(x0 + 1, field.width - 1);
            const int y0 = std::clamp(static_cast<int>(std::floor(tile_y)), 0, field.height - 1);
            const int y1 = std::min(y0 + 1, field.height - 1);
            const float fx = tile_x - static_cast<float>(x0);
            const float fy = tile_y - static_cast<float>(y0);
            const auto sample = [&](const std::vector<float>& layer) {
                const float top = field.at(layer, x0, y0) * (1.0f - fx) +
                                  field.at(layer, x1, y0) * fx;
                const float bottom = field.at(layer, x0, y1) * (1.0f - fx) +
                                     field.at(layer, x1, y1) * fx;
                return top * (1.0f - fy) + bottom * fy;
            };
            const float coverage_field = sample(field.cloud);
            const float rain_field = sample(field.rain);
            const float snow_field = sample(field.snow);
            const float wind_u = sample(field.wind_u);
            const float wind_v = sample(field.wind_v);
            const float temperature = sample(field.temperature);

            // Advected fBm: wind displaces the noise lookup over time.
            const float advect_x = static_cast<float>(field.time_s) * wind_u * 0.08f;
            const float advect_y = static_cast<float>(field.time_s) * wind_v * 0.08f;
            const float noise = fbm(
                static_cast<float>(px) * 0.06f - advect_x * static_cast<float>(texels),
                static_cast<float>(py) * 0.06f - advect_y * static_cast<float>(texels),
                lattice_period, field.seed);
            float cloud_density = smoothstep(0.35f, 0.75f,
                coverage_field * 0.65f + noise * 0.35f);
            if (cloud_density <= 0.0f && rain_field <= 0.0f && snow_field <= 0.0f) continue;

            // Precipitation: gated streaks under clouds, slanted by wind.
            // The band phase and dither hash are X-periodic at the image
            // width so the wrap meridian never shows a step (WP3 seam test).
            const bool precipitation = (rain_field > 0.05f || snow_field > 0.05f) &&
                                       cloud_density > 0.15f;
            const float slant = wind_u * 2.0f;
            const float fall = std::fmod(
                static_cast<float>(field.time_s) * 6.0f +
                hash_float(hash_u32(field.seed ^
                    static_cast<std::uint32_t>(px % lattice_period) * 73856093u)) * 97.0f, 12.0f);
            float rain_alpha = 0.0f;
            if (precipitation && rain_field > snow_field) {
                const float phase_px = std::fmod(
                    static_cast<float>(px) +
                    static_cast<float>(lattice_period) * 64.0f,  // shift into positive range
                    static_cast<float>(lattice_period));
                const float band = std::fmod(
                    phase_px * slant + static_cast<float>(py) + fall * (1.0f + snow_field), 3.0f);
                if (band < 0.9f && rain_field > 0.25f)
                    rain_alpha = 0.42f * std::min(1.0f, rain_field * 1.4f);
            }
            // Snow: dithered white dots.
            float snow_alpha = 0.0f;
            if (precipitation && snow_field > 0.05f && temperature < 0.5f) {
                const float dither = hash_float(hash_u32(field.seed ^
                    static_cast<std::uint32_t>(px % lattice_period) * 19349663u ^
                    static_cast<std::uint32_t>(static_cast<int>(fall)) * 83492791u));
                if (dither < 0.10f + snow_field * 0.15f)
                    snow_alpha = 0.65f;
            }

            QColor color;
            int alpha_band = posterize(cloud_density);
            if (alpha_band > 0 || precipitation) {
                // Cloud shading: lighter top edge, darker underside, from the
                // vertical noise gradient; darker base for storm-heavy fields.
                const float shade = 0.5f + 0.5f * std::clamp(
                    noise + (params.sun_dy >= 0.0f ? -0.25f : 0.25f), 0.0f, 1.0f);
                int base = static_cast<int>(150.0f + 60.0f * shade);
                if (rain_field > 0.5f) base = std::max(70, base - 40);
                const float day = 1.0f - darkness;
                color = QColor::fromRgb(
                    static_cast<int>(std::clamp(base * day, 0.0f, 255.0f)),
                    static_cast<int>(std::clamp((base - 6) * day, 0.0f, 255.0f)),
                    static_cast<int>(std::clamp((base + 4) * day, 0.0f, 255.0f)));
                const int alpha = 60 + alpha_band * 38;
                color.setAlpha(std::min(255, alpha));
            } else {
                continue;
            }
            // Cloud shadow pass is baked as a dark offset sample first.
            if (params.shadows && cloud_density > 0.3f) {
                const int shadow_px = px - static_cast<int>(params.sun_dx * texels);
                const int shadow_py = py - static_cast<int>(params.sun_dy * texels);
                if (shadow_px >= 0 && shadow_px < image_width &&
                    shadow_py >= 0 && shadow_py < image_height &&
                    alpha_band < 2) {
                    // Cheap approximation: darken below the cloud mass.
                    color = color.darker(120);
                }
            }
            if (rain_alpha > 0.0f) {
                const QColor rain_color(70, 130, 165,
                                        static_cast<int>(rain_alpha * 255.0f));
                color = rain_color;
            } else if (snow_alpha > 0.0f) {
                color = QColor(239, 249, 255, static_cast<int>(snow_alpha * 255.0f));
            }
            image.setPixel(px, py, color.rgba());
        }
    }
    return image;
}

} // namespace aur::world
