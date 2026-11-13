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
