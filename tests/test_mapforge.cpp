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

    // Coastal beach painting keeps water, beach, and grass aligned to the picked edge.
    Json terra(JsonObject{});
    JsonArray terra_tiles;
    auto push_named = [&](const std::string &tname) {
        Json rec(JsonObject{});
        rec["name"] = tname;
        rec["flags"] = 0;
        terra_tiles.push_back(std::move(rec));
    };
    push_named("grass");
    push_named("grass2");
    for (int i = 2; i < 40; ++i)
        push_named(i >= 4 && i < 20 ? "water_" + std::to_string(i - 4)
                 : i >= 20 && i < 36 ? "field_beach_" + std::to_string(i - 20)
                 : "misc");
    terra["tiles"] = terra_tiles;
    JsonArray water_vars, beach_vars;
    for (int i = 0; i < 16; ++i) {
        water_vars.push_back(Json(4 + i));
        beach_vars.push_back(Json(20 + i));
    }
    Json water_grp(JsonObject{});
    water_grp["edge_joined"] = true;
    water_grp["joins"] = JsonArray{Json("water")};
    water_grp["tiles"] = water_vars;
    Json beach_grp(JsonObject{});
    beach_grp["edge_joined"] = true;
    beach_grp["joins"] = JsonArray{Json("field_beach")};
    beach_grp["tiles"] = beach_vars;
    Json terra_auto(JsonObject{});
    terra_auto["water"] = water_grp;
    terra_auto["field_beach"] = beach_grp;
    terra["autotiles"] = terra_auto;

    TerrainPick west_pick = pick;
    GeneratedMap west_gen = generate_map_asset(MapKind::Town, 7, 12, 8, 32, 40,
                                               "west_beach", "West Beach", &west_pick);
    apply_coastal_beach(west_gen.map, terra, west_pick, 7);
    const JsonArray &wg = west_gen.map["layers"]["ground"][4].as_array();
    ASSERT(wg[0].as_int() >= 4 && wg[0].as_int() < 20);
    ASSERT(wg[1].as_int() >= 4 && wg[1].as_int() < 20);
    ASSERT(wg[2].as_int() == 20 + (N | E | S));
    ASSERT(wg[3].as_int() == 20 + (N | S | W));
    ASSERT(wg[4].as_int() == 0 || wg[4].as_int() == 1);

    TerrainPick east_pick;
    east_pick.group = "ocean";
    east_pick.land_mask = N | S | W;
    GeneratedMap east_gen = generate_map_asset(MapKind::Town, 7, 12, 8, 32, 40,
                                               "east_beach", "East Beach", &east_pick);
    apply_coastal_beach(east_gen.map, terra, east_pick, 7);
    const JsonArray &eg = east_gen.map["layers"]["ground"][4].as_array();
    ASSERT(eg[11].as_int() >= 4 && eg[11].as_int() < 20);
    ASSERT(eg[10].as_int() >= 4 && eg[10].as_int() < 20);
    ASSERT(eg[9].as_int() == 20 + (N | S | W));
    ASSERT(eg[8].as_int() == 20 + (N | E | S));
    ASSERT(eg[7].as_int() == 0 || eg[7].as_int() == 1);
}
