#include "WorldMapHelpers.hpp"

#include "aur/clock.hpp"

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

    std::string rolled_weather(const Project *project, const Json &map, int day, int hour) {
        if (!project) return "clear";
        static const Json empty;
        const auto entry = project->meta.find("weather");
        const Json &settings = entry != project->meta.end() ? entry->second : empty;
        const int interval = settings.contains("interval_hours") && settings["interval_hours"].is_number()
                                 ? std::max(1, settings["interval_hours"].as_int()) : 3;
        const std::string climate = map["climate"].is_string() ? map["climate"].as_string()
                                                               : std::string("temperate");
        return aur::weather_for_slot(settings, climate, DEFAULT_WEATHER_SEED, day, hour / interval);
    }

} // namespace aur::world
