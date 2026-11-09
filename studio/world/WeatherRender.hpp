#pragma once

// WP3 weather renderer v2 (work packages brief): a pure, deterministic
// renderer that turns a weather field into one low-res image drawn once with
// nearest-neighbour scaling. No per-frame particle loops, no per-cell hash
// popping. Headless-testable via a golden FNV-1a checksum.

#include "aur/json.hpp"

#include <QColor>
#include <QImage>
#include <QRgb>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aur::world {

// Map-tile resolution field, row-major, width*height floats in 0..1
// (wind components in -1..1).
struct WeatherField {
    int width = 0;
    int height = 0;
    std::vector<float> cloud;
    std::vector<float> rain;
    std::vector<float> snow;
    std::vector<float> wind_u;
    std::vector<float> wind_v;
    std::vector<float> temperature;
    std::uint32_t seed = 1;
    double time_s = 0.0;  // drives advection and rain phase

    bool valid() const {
        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        return width > 0 && height > 0 && cloud.size() == count;
    }
    float at(const std::vector<float>& layer, int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return layer[static_cast<size_t>(y) * static_cast<size_t>(width) +
                     static_cast<size_t>(x)];
    }
};

struct WeatherRenderParams {
    double hour = 8.0;
    int texels_per_tile = 4;          // lower for huge maps; cap image ~1 MP
    QRgb ambient = 0xffffffff;
    float ambient_darkness = 0.0f;    // 0..1, dims the whole render
    float sun_dx = -0.6f;             // cloud shadow direction (texels)
    float sun_dy = 0.8f;
    bool shadows = true;
};

// Deterministic: same field + params + seed give the same pixels.
QImage render_weather_image(const WeatherField& field, const WeatherRenderParams& params);

} // namespace aur::world
