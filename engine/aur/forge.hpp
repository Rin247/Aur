#pragma once

#include "json.hpp"
#include "gfx.hpp"
#include <string>
#include <vector>
#include <map>
#include <utility>

namespace aur {

enum class RaceGroup { Mortal, Supernatural };

struct RaceInfo {
    std::string id;
    std::string label;
    RaceGroup group;
    bool uses_affinity;
};

Json build_palette(const Json& colors);
Json compile_character(const Json& spec, const Json& parts = Json{});

std::vector<RaceInfo> character_race_catalog();
std::vector<std::pair<std::string, std::string>> character_race_options();
std::vector<std::pair<std::string, std::string>> character_variant_options(const std::string& race);
std::vector<std::pair<std::string, std::string>> alignment_options();
std::vector<std::pair<std::string, std::string>> affinity_options();
std::vector<std::pair<std::string, std::string>> entity_role_options();
std::vector<std::pair<std::string, std::string>> hair_style_options();
std::vector<std::pair<std::string, std::string>> accessory_options();

bool is_supported_character_race(const std::string& race);
bool is_valid_alignment(const std::string& alignment);
bool is_valid_affinity(const std::string& affinity);
Json random_character_spec(int seed, const std::string& name, const std::string& age_group = "adult",
                           const std::string& gender = "unspecified",
                           const std::string& presentation = "androgynous",
                           const std::string& race = "kaeli",
                           const std::string& affinity = "random",
                           const std::string& alignment = "unaligned",
                           const std::string& role = "npc",
                           const std::string& variant = "random");
Json generate_creature_graphic(const std::string& species, int seed);
Json generate_enemy_graphic(const std::string& kind, int seed);
Json generate_effect_graphic(const std::string& effect_kind, int seed);
Json generate_clothing_graphic(const std::string& kind, int seed);
Json generate_prop_graphic(const std::string& kind, int seed);
Json generate_tile_asset(const std::string& kind, int seed, int width = 16, int height = 16,
                         int base = 2, int dark = 3, int light = 4, int accent = 5);
JsonArray generate_starter_world_terrain_tiles(int palette_count);
JsonArray generate_field_beach_tiles(int palette_index = 0);
void register_parts(const Json& parts);

} // namespace aur
