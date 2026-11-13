#include "mini_test.hpp"

#include "aur/mapforge.hpp"
#include "aur/project.hpp"

#include <string>

// The seeded map: the same seed gives the same map, the kinds differ,
// and the platformer's floor is solid while the field's grass is not.
void test_mapforge()
{
    using namespace aur;

    MapKind kind = MapKind::Town;
    ASSERT(mapKindFromName("town", kind) && kind == MapKind::Town);
    ASSERT(mapKindFromName("cave", kind) && kind == MapKind::Cave);
    ASSERT(mapKindFromName("platformer", kind) && kind == MapKind::Platformer);
    ASSERT(!mapKindFromName("nonsense", kind));
    ASSERT(mapKindName(MapKind::Platformer) == "platformer");

    // The same seed gives the same map.
    const GeneratedMap first = generate_map_asset(MapKind::Town, 7, 20, 12, 16, 100,
                                                 "town_a", "Town A");
    const GeneratedMap again = generate_map_asset(MapKind::Town, 7, 20, 12, 16, 100,
                                                 "town_a", "Town A");
    ASSERT(dump_json(first.map) == dump_json(again.map));

    // Another seed gives another map.
    const GeneratedMap other = generate_map_asset(MapKind::Town, 8, 20, 12, 16, 100,
                                                  "town_a", "Town A");
    ASSERT(dump_json(first.map) != dump_json(other.map));

    // A town: the mode is the field's, the edges are walls.
    ASSERT(first.map["mode"].as_string() == "action");
    ASSERT(first.map["w"].as_int() == 20);
    ASSERT(first.map["h"].as_int() == 12);
    const Json &ground = first.map["layers"]["ground"];
    ASSERT(ground.as_array().size() == 12);
    ASSERT(ground.as_array()[0].as_array()[0].as_int() == 101);
    ASSERT(ground.as_array()[1].as_array()[1].as_int() == 100);

    // The tiles it brings: four of them, the wall among them solid.
    ASSERT(first.tiles.size() == 4);
    ASSERT(first.tiles[1]["flags"].as_int() == aur::SOLID);
    ASSERT(first.tiles[0]["flags"].as_int() == 0);

    // A platformer room: the mode is the platformer's, the floor is solid
    // and the field's grass is not in the map at all.
    const GeneratedMap room = generate_map_asset(MapKind::Platformer, 3, 24, 14, 16, 100,
                                                 "room_a", "Room A");
    ASSERT(room.map["mode"].as_string() == "platformer");
    ASSERT(room.tiles[2]["flags"].as_int() == aur::SOLID);
    ASSERT(room.tiles[3]["flags"].as_int() == aur::ONE_WAY);
    const Json &room_ground = room.map["layers"]["ground"];
    // The floor is the bottom two rows, and it is the solid tile.
    ASSERT(room_ground.as_array()[13].as_array()[0].as_int() == 102);
    ASSERT(room_ground.as_array()[0].as_array()[0].as_int() == -1);
    // The platforms are in the objects layer.
    bool has_platform = false;
    for (const Json &row : room.map["layers"]["objects"].as_array())
    {
        for (const Json &cell : row.as_array())
        {
            if (cell.as_int() == 103) has_platform = true;
        }
    }
    ASSERT(has_platform);

    // The spawn is on the floor.
    bool has_spawn = false;
    for (const Json &entity : room.map["entities"].as_array())
    {
        if (entity["type"].as_string() == "spawn") has_spawn = true;
    }
    ASSERT(has_spawn);

    // The sizes are checked.
    bool threw = false;
    try {
        generate_map_asset(MapKind::Town, 1, 2, 12, 16, 0, "x", "X");
    } catch (const std::exception &) {
        threw = true;
    }
    ASSERT(threw);
}

// The globe's tile: the terrain it stands for and the side its land is
// on, and an expansion that keeps that side.
void test_globe_pick()
{
    using namespace aur;

    // A tileset with an ocean's autotile: sixteen variants, and a land
    // tile that is not in the group.
    Json tileset = Json(JsonObject{});
    JsonArray tiles;
    for (int index = 0; index < 16; ++index)
    {
        Json tile = Json(JsonObject{});
        tile["name"] = "ocean_" + std::to_string(index);
        tile["flags"] = 0;
        tiles.push_back(tile);
    }
    Json land = Json(JsonObject{});
    land["name"] = "grass";
    land["flags"] = 0;
    tiles.push_back(land);
    tileset["tiles"] = tiles;
    JsonArray variants;
    for (int index = 0; index < 16; ++index)
        variants.push_back(Json(index));
    Json autotile = Json(JsonObject{});
    autotile["edge_joined"] = true;
    autotile["joins"] = JsonArray{Json("ocean")};
    autotile["tiles"] = variants;
    Json autotiles = Json(JsonObject{});
    autotiles["ocean"] = autotile;
    tileset["autotiles"] = autotiles;

    // A map whose middle cell is the ocean's edge, with the ocean to its
    // west and the land on the other three sides.
    const int ocean_variant = 8;
    const int land_tile = 16;
    Json map = Json(JsonObject{});
    map["w"] = 3;
    map["h"] = 3;
    JsonArray rows;
    for (int y = 0; y < 3; ++y)
    {
        JsonArray row;
        for (int x = 0; x < 3; ++x)
            row.push_back(Json(y == 1 && x <= 1 ? ocean_variant : land_tile));
        rows.push_back(row);
    }
    Json layers = Json(JsonObject{});
    layers["ground"] = rows;
    map["layers"] = layers;

    // The pick: the ocean, and the land on the three sides the ocean is
    // not.
    const TerrainPick pick = pick_terrain(map, tileset, "ground", 1, 1);
    ASSERT(pick.group == "ocean");
    ASSERT(pick.tile_name == "ocean_8");
    ASSERT((pick.land_mask & aur::W) == 0);   // the ocean is on the west
    ASSERT((pick.land_mask & aur::N) != 0);
    ASSERT((pick.land_mask & aur::E) != 0);
    ASSERT((pick.land_mask & aur::S) != 0);

    // The expansion keeps the side: the west edge is the wall, the rest
    // is the land.
    const GeneratedMap expanded = generate_map_asset(MapKind::Town, 5, 12, 8, 16, 100,
                                                     "expanded", "Expanded", &pick);
    const Json &ground = expanded.map["layers"]["ground"];
    // The west is the ocean's side, so the west edge is the wall; the land
    // is on the north, the east and the south.
    ASSERT(ground.as_array()[0].as_array()[0].as_int() == 101);
    ASSERT(ground.as_array()[4].as_array()[0].as_int() == 101);
    ASSERT(ground.as_array()[4].as_array()[11].as_int() == 100);
    ASSERT(ground.as_array()[0].as_array()[11].as_int() == 100);
}
