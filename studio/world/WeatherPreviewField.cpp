#include "WeatherPreviewField.hpp"
#include "WorldMapHelpers.hpp"
#include "aur/project.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace aur::world {

namespace {

// Low-frequency coherent noise on the tile lattice (smooth, not per-cell hash).
float coherent(const WeatherField& field, int x, int y, std::uint32_t salt,
               double frequency) {
    const double fx = static_cast<double>(x) * frequency;
    const double fy = static_cast<double>(y) * frequency;
    const double t = 0.5 * (std::sin(2.0 * fx + salt) * std::cos(1.7 * fy - salt) +
                            std::cos(0.9 * fx - salt * 0.5) * std::sin(1.3 * fy + salt * 0.5));
    const double wrapped = 0.5 + 0.5 * t;
    return static_cast<float>(std::clamp(wrapped, 0.0, 1.0));
}

// Latitude bands drive the default wind and temperature when no layers exist.
float latitude_of(int y, int height) {
    return height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.5f;
}

} // namespace

WeatherField make_preview_field(const Project* project, const Json& map,
                                const std::string& weather,
                                std::uint32_t seed, double time_s) {
    WeatherField field;
    if (!project || !map.contains("w") || !map.contains("h")) return field;
    field.width = map["w"].as_int();
    field.height = map["h"].as_int();
    if (field.width <= 0 || field.height <= 0) {
        field.width = 0;
        field.height = 0;
        return field;
    }
    const size_t count = static_cast<size_t>(field.width) * static_cast<size_t>(field.height);
    field.cloud.assign(count, 0.0f);
    field.rain.assign(count, 0.0f);
    field.snow.assign(count, 0.0f);
    field.wind_u.assign(count, 0.0f);
    field.wind_v.assign(count, 0.0f);
    field.temperature.assign(count, 0.5f);
    field.seed = seed;
    field.time_s = time_s;

    const JsonArray* biome_rows = map.contains("layers") && map["layers"].contains("biomes") &&
        map["layers"]["biomes"].is_array() ? &map["layers"]["biomes"].as_array() : nullptr;
    const JsonArray* temperature_rows = map.contains("layers") &&
        map["layers"].contains("temperature") && map["layers"]["temperature"].is_array()
        ? &map["layers"]["temperature"].as_array() : nullptr;

    for (int y = 0; y < field.height; ++y) {
        for (int x = 0; x < field.width; ++x) {
            const size_t index = static_cast<size_t>(y) * static_cast<size_t>(field.width) +
                                 static_cast<size_t>(x);
            const float latitude = latitude_of(y, field.height);

            // Wind: easterly/westerly trade bands, coherent with latitude.
            const float band = std::sin(static_cast<float>(latitude) * 3.14159265f * 2.0f);
            field.wind_u[index] = std::clamp(band * 0.8f, -1.0f, 1.0f);
            field.wind_v[index] = 0.15f * std::cos(latitude * 9.42f);

            // Temperature 0..1 from the authored layer, else latitude bands.
            float temperature = 1.0f - std::abs(latitude * 2.0f - 1.0f);
            if (temperature_rows && y < static_cast<int>(temperature_rows->size()) &&
                (*temperature_rows)[static_cast<size_t>(y)].is_array() &&
                x < static_cast<int>((*temperature_rows)[static_cast<size_t>(y)].as_array().size())) {
                temperature = std::clamp(
                    (*temperature_rows)[static_cast<size_t>(y)][static_cast<size_t>(x)].as_int() / 100.0f,
                    0.0f, 1.0f);
            }
            field.temperature[index] = temperature;

            // Biome climate for the profile mix.
            std::string climate;
            if (biome_rows && y < static_cast<int>(biome_rows->size()) &&
                (*biome_rows)[static_cast<size_t>(y)].is_array() &&
                x < static_cast<int>((*biome_rows)[static_cast<size_t>(y)].as_array().size())) {
                const std::string biome_id = (*biome_rows)[static_cast<size_t>(y)]
                    [static_cast<size_t>(x)].as_string();
                const auto biome = project->biomes.find(biome_id);
                if (biome != project->biomes.end() && biome->second["climate"].is_string())
                    climate = biome->second["climate"].as_string();
            }

            const auto mix_from_profile = [&](float base) {
                // climate mode: type shares from the project's weather profile.
                float cloudy = base;
                float rainy = 0.0f;
                float snowy = 0.0f;
                if (project->meta.contains("weather") &&
                    project->meta.at("weather").is_object()) {
                    const Json& weather_meta = project->meta.at("weather");
                    if (weather_meta.contains("profiles") &&
                        weather_meta["profiles"].is_object()) {
                    const Json& profiles = weather_meta["profiles"];
                    if (profiles.contains(climate) && profiles[climate].is_object()) {
                        const Json& profile = profiles[climate];
                        const auto share = [&](const char* key) {
                            return profile.contains(key) && profile[key].is_number()
                                ? std::clamp(static_cast<float>(profile[key].as_number()), 0.0f, 1.0f) : 0.0f;
                        };
                        cloudy = std::max(base, share("cloudy"));
                        rainy = share("rain");
                        snowy = share("snow");
                    }
                    }
                }
                snowy *= (1.0f - temperature);  // snow only in cold regions
                return std::array<float, 3>{cloudy, rainy, snowy};
            };

            std::array<float, 3> amounts{0.0f, 0.0f, 0.0f};
            if (weather == "climate") {
                const float variation = coherent(field, x, y, seed, 0.09);
                amounts = mix_from_profile(0.25f + 0.6f * variation);
            } else if (weather == "cloudy") {
                amounts[0] = 0.55f + 0.35f * coherent(field, x, y, seed, 0.07);
            } else if (weather == "rain" || weather == "storm") {
                const float strength = weather == "storm" ? 0.95f : 0.7f;
                amounts[0] = strength;
                amounts[1] = strength * (0.6f + 0.4f * coherent(field, x, y, seed, 0.11));
            } else if (weather == "snow") {
                amounts[0] = 0.6f;
                amounts[2] = 0.5f + 0.4f * coherent(field, x, y, seed, 0.11);
                amounts[2] *= (1.0f - temperature);
            } else if (weather == "fog") {
                amounts[0] = 0.25f * coherent(field, x, y, seed, 0.13);
            }
            field.cloud[index] = amounts[0];
            field.rain[index] = amounts[1];
            field.snow[index] = amounts[2];
        }
    }
    return field;
}

} // namespace aur::world
