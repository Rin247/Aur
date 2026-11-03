#pragma once

#include "json.hpp"

#include <stdexcept>

namespace aur {

inline bool wraps_world_x(const Json& map) {
    if (!map.contains("worldgen") || !map["worldgen"].is_object() ||
        !map["worldgen"].contains("topology") ||
        !map["worldgen"]["topology"].is_string()) {
        return false;
    }
    const std::string& topology = map["worldgen"]["topology"].as_string();
    return topology == "continents" || topology == "archipelago" ||
        topology == "supercontinent" || topology == "fractured";
}

inline int wrapX(int x, int width) {
    if (width <= 0) throw std::invalid_argument("World width must be positive");
    const int remainder = x % width;
    return remainder < 0 ? remainder + width : remainder;
}

inline int worldX(const Json& map, int x, int width) {
    return wraps_world_x(map) ? wrapX(x, width) : x;
}

inline int wrapped_delta_x(int from, int to, int width) {
    if (width <= 0) throw std::invalid_argument("World width must be positive");
    int delta = to - from;
    if (delta > width / 2) delta -= width;
    else if (delta < -width / 2) delta += width;
    return delta;
}

} // namespace aur
