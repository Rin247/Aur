#pragma once

#include "aur/json.hpp"
#include "aur/project.hpp"

#include <cstdint>
#include <string>

namespace aur::world
{

    std::string make_biome_id(const std::string &name);

// The weather the game rolls at a day and hour on this map: the engine's
// own pick, so the preview and the game agree.
std::string rolled_weather(const Project *project, const Json &map, int day, int hour);
    bool has_hd_surface(const Json &map);
    uint32_t surface_hash(uint32_t seed, int x, int y, uint32_t layer);
    std::string terrain_tile_category(const std::string &name);
    
} // namespace aur::world
