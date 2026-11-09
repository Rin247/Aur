#pragma once

// WP3: builds a spatially coherent preview WeatherField for the World Map
// editor's weather overlay. Forced weather types use low-frequency noise (not
// per-cell hash, which popped); "climate" mixes types by the project's weather
// profiles per biome climate, blended across biome boundaries. This is the
// existing Preview behaviour made good — Live mode (WP4) replaces the field
// entirely and this stays Preview-only.

#include "WeatherRender.hpp"

#include <QString>
#include <string>

namespace aur {

class Project;
class Json;

namespace world {

// `weather` is one of: climate | clear | cloudy | rain | snow | storm | fog.
WeatherField make_preview_field(const Project* project, const Json& map,
                                const std::string& weather,
                                std::uint32_t seed, double time_s);

} // namespace world
} // namespace aur
