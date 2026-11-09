#include "WorldMapHelpers.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace aur::world
{

    std::string make_biome_id(const std::string &name)
    {
        std::string id;
        bool separator = false;
        for (unsigned char ch : name) {
            if (std::isalnum(ch)) {
                if (separator && !id.empty()) id.push_back('_');
                id.push_back(static_cast<char>(std::tolower(ch)));
                separator = false;
            } else {
                separator = true;
            }
        }
        return id;
    }

    bool has_hd_surface(const Json &map)
    {
        if (!map.is_object()) return false;
        const auto& fields = map.as_object();
        const auto worldgen = fields.find("worldgen");
        if (worldgen == fields.end() || !worldgen->second.is_object()) return false;
        const auto& worldgen_fields = worldgen->second.as_object();
        const auto style = worldgen_fields.find("style");
        if (style == worldgen_fields.end() || !style->second.is_string() ||
            style->second.as_string() != "hd2d") return false;
        const auto layers = fields.find("layers");
        if (layers == fields.end() || !layers->second.is_object()) return false;
        const auto& layer_fields = layers->second.as_object();
        const auto elevation = layer_fields.find("elevation");
        return elevation != layer_fields.end() && elevation->second.is_array();
    }

    uint32_t surface_hash(uint32_t seed, int x, int y, uint32_t layer)
    {
        uint32_t value = seed ^ static_cast<uint32_t>(x) * 0x9e3779b9u ^
                         static_cast<uint32_t>(y) * 0x85ebca6bu ^ layer;
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        return value ^ (value >> 16);
    }

    std::string terrain_tile_category(const std::string &name)
    {
        std::string normalized = name;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });
        if (normalized.find("biome_") == 0) return "biomes";
        const auto contains_any = [&normalized](std::initializer_list<const char *> terms) {
            return std::any_of(terms.begin(), terms.end(), [&normalized](const char *term) {
                return normalized.find(term) != std::string::npos;
            });
        };
        if (contains_any({"shore", "coast", "beach", "sand"})) return "coast";
        if (contains_any({"ocean", "water", "sea", "reef"})) return "underwater";
        if (contains_any({"mountain", "hill", "cliff", "rock", "stone", "snow", "scree"})) {
            return "elevation";
        }
        if (contains_any({"grass", "forest", "foliage", "jungle", "moss"})) return "surface";
        return "structures";
    }

    std::string climate_weather(const Project *project, const std::string &climate,
                                uint32_t roll)
    {
        if (!project) return "clear";
        const auto& meta_fields = project->meta;
        const auto weather_it = meta_fields.find("weather");
        if (weather_it == meta_fields.end()) return "clear";
        const Json &weather_settings = weather_it->second;
        if (!weather_settings.is_object()) return "clear";
        const auto& weather_fields = weather_settings.as_object();
        const auto profiles_it = weather_fields.find("profiles");
        if (profiles_it == weather_fields.end()) return "clear";
        const Json &profiles = profiles_it->second;
        if (!profiles.is_object()) return "clear";
        const auto& profile_map = profiles.as_object();
        const auto profile_it = profile_map.find(climate);
        const auto temperate_it = profile_map.find("temperate");
        const Json* profile = profile_it != profile_map.end() ? &profile_it->second : nullptr;
        if ((!profile || !profile->is_object()) && temperate_it != profile_map.end()) {
            profile = &temperate_it->second;
        }
        if (!profile || !profile->is_object()) return "clear";

        int total_weight = 0;
        for (const auto& [_, weight] : profile->as_object()) {
            if (weight.is_number() && weight.as_int() > 0) total_weight += weight.as_int();
        }
        if (total_weight <= 0) return "clear";

        int selection = static_cast<int>(roll % static_cast<uint32_t>(total_weight));
        for (const auto& [weather, weight] : profile->as_object()) {
            if (!weight.is_number() || weight.as_int() <= 0) continue;
            selection -= weight.as_int();
            if (selection < 0) return weather;
        }
        return "clear";
    }

} // namespace aur::world
