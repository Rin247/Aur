#pragma once

#include "aur/json.hpp"
#include "aur/project.hpp"

#include <cstdint>
#include <string>

namespace aur::world
{

    std::string make_biome_id(const std::string &name);
    bool has_hd_surface(const Json &map);
    uint32_t surface_hash(uint32_t seed, int x, int y, uint32_t layer);
    std::string terrain_tile_category(const std::string &name);
    std::string climate_weather(const Project *project, const std::string &climate, uint32_t roll);

} // namespace aur::world
