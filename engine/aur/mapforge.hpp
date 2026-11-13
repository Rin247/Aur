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
#include "aur/autotile.hpp"

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

    // What a tile on a world map is: the terrain it stands for, and the
    // sides the land is on. The globe is the coarse picture of the place
    // (8-bit); the map the expansion makes is the fine one (16/32-bit),
    // so the land keeps the side it had.
    struct TerrainPick
    {
        // The autotile's group ("ocean", "sand", "grass") and the tile's
        // name, when the tileset names it.
        std::string group;
        std::string tile_name;
        // The sides the land is on: N, E, S or W, bit for bit.
        int land_mask = 0;
        int x = 0;
        int y = 0;
    };

    // What the cell of a map holds: the terrain and the sides the land is
    // on. The mask comes from the autotile's variants, so a coastal cell
    // says which side its beach is on.
    TerrainPick pick_terrain(const Json &map, const Json &tileset, const std::string &layer,
                              int x, int y);

    // The map: its layers, its entities and its mode. The tile indices
    // start at `first_tile_index`, which is the tileset's current size.
    // `pick` is what the picked tile was: the expansion keeps its terrain
    // and its sides.
    GeneratedMap generate_map_asset(MapKind kind, int seed, int width, int height,
                                    int tile_size, int first_tile_index,
                                    const std::string &map_id, const std::string &name,
                                    const TerrainPick *pick = nullptr);

} // namespace aur
