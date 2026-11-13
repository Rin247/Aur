#pragma once

// Mapforge.hpp - the seeded map generator.
//
// The estate can seed a character, a creature, an effect, clothing, a
// prop and a tile. This makes a map: a town's streets, a cave, or a
// platformer room, with the tiles it needs.
//
// The generator brings its own tiles, because the tileset's flags are per
// tile: a platformer's floor cannot be the field's grass, or the grass
// would be solid in every map that uses it.

#include <string>
#include <vector>

#include "aur/json.hpp"

namespace aur
{

    enum class MapKind
    {
        Town,
        Cave,
        Platformer
    };

    // The kind's name, and the name's kind.
    std::string mapKindName(MapKind kind);
    bool mapKindFromName(const std::string &name, MapKind &kind);

    // A generated map and the tiles it needs.
    struct GeneratedMap
    {
        Json map;
        // The tiles to append to the tileset, in order.
        JsonArray tiles;
    };

    // The map: its layers, its entities and its mode. The tile indices
    // start at `first_tile_index`, which is the tileset's current size.
    GeneratedMap generate_map_asset(MapKind kind, int seed, int width, int height,
                                    int tile_size, int first_tile_index,
                                    const std::string &map_id, const std::string &name);

} // namespace aur
