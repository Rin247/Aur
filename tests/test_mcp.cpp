#include "mini_test.hpp"
#include "aur/mcp.hpp"

#include <chrono>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

void test_mcp_server() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("aur_mcp_test_" + std::to_string(unique));
    fs::create_directories(root);
    struct TempDirectory {
        fs::path path;
        ~TempDirectory() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{root};

    aur::Project project(root.string());
    project.meta["title"] = "MCP Test";

    aur::Json tile;
    tile["name"] = "grass";
    tile["rows"] = aur::JsonArray(16, aur::Json(std::string(16, '1')));
    tile["flags"] = 0;
    tile["pal"] = 0;

    aur::Json tileset;
    tileset["name"] = "World";
    tileset["tile"] = 16;
    tileset["tiles"] = aur::JsonArray(8, tile);
    aur::JsonArray palette;
    for (int index = 0; index < 16; ++index) palette.emplace_back("#000000");
    tileset["palettes"] = aur::JsonArray{aur::Json(palette)};
    project.tilesets["world"] = tileset;

    aur::Json map;
    map["id"] = "overworld";
    map["name"] = "Overworld";
    map["kind"] = "world";
    map["w"] = 2;
    map["h"] = 2;
    map["tileset"] = "world";
    aur::Json layers;
    const aur::Json row = aur::JsonArray{aur::Json(0), aur::Json(0)};
    layers["ground"] = aur::JsonArray{row, row};
    layers["objects"] = aur::JsonArray{aur::JsonArray{aur::Json(-1), aur::Json(-1)},
                                         aur::JsonArray{aur::Json(-1), aur::Json(-1)}};
    layers["above"] = layers["objects"];
    map["layers"] = layers;
    map["entities"] = aur::JsonArray{};
    project.maps["overworld"] = map;
    project.save();

    std::stringstream input;
    input << R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05"}})" << '\n';
    input << R"({"jsonrpc":"2.0","method":"notifications/initialized"})" << '\n';
    input << R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})" << '\n';
    input << R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"map_paint","arguments":{"map_id":"overworld","layer":"ground","edits":[{"x":1,"y":0,"tile":0}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"palette_set","arguments":{"tileset_id":"world","palette_index":0,"color_index":1,"color":"#3f8f45"}}})" << '\n';
    std::ostringstream tile_call;
    tile_call << R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"tile_set","arguments":{"tileset_id":"world","tile_index":0,"tile":{"name":"grass-updated","rows":[)";
    for (int row = 0; row < 16; ++row) {
        if (row > 0) tile_call << ',';
        tile_call << '"' << std::string(16, '1') << '"';
    }
    tile_call << R"(],"flags":0,"pal":0}}}})";
    input << tile_call.str() << '\n';
    input << R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"graphic_put","arguments":{"graphic_id":"hero","graphic":{"w":1,"h":1,"palette":["#000000","#111111","#222222","#333333","#444444","#555555","#666666","#777777","#888888","#999999","#aaaaaa","#bbbbbb","#cccccc","#dddddd","#eeeeee","#ffffff"],"frames":{"idle_down":["."]},"anims":{},"origin":[0,1]}}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"character_put","arguments":{"character_id":"ari","character":{"name":"Ari","role":"hero"}}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"creature_put","arguments":{"creature_id":"moth","creature":{"name":"Glimmer Moth","hp":12}}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"map_paint","arguments":{"map_id":"overworld","layer":"ground","edits":[{"x":8,"y":0,"tile":0}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"biome_put","arguments":{"biome_id":"coastal_mist","biome":{"name":"Coastal Mist","climate":"temperate","tileset":"world","base_tile":0,"color":"#55aacc","vegetation":30,"decay":50,"water_level":80}}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"biome_apply","arguments":{"map_id":"overworld","edits":[{"x":0,"y":1,"biome_id":"coastal_mist"}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"world_generate","arguments":{"map_id":"new_world","name":"New World","width":24,"height":16,"seed":2718,"topology":"archipelago","water_level":52,"vegetation":67,"decay":12}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":18,"method":"tools/call","params":{"name":"map_paint","arguments":{"map_id":"new_world","layer":"terrain_overrides","edits":[{"x":1,"y":1,"tile":2}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":19,"method":"tools/call","params":{"name":"biome_apply","arguments":{"map_id":"new_world","edits":[{"x":2,"y":2,"biome_id":"coastal_mist"}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":13,"method":"tools/call","params":{"name":"character_generate","arguments":{"character_id":"child_guide","name":"Child Guide","age_group":"child","race":"featherfolk","gender":"non-binary","presentation":"feminine","seed":12345}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":14,"method":"tools/call","params":{"name":"creature_generate","arguments":{"creature_id":"house_cat","name":"House Cat","species":"cat","seed":27}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":15,"method":"tools/call","params":{"name":"effect_generate","arguments":{"graphic_id":"healing_spark","kind":"heal","seed":87}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":16,"method":"tools/call","params":{"name":"tile_generate","arguments":{"tileset_id":"world","tile_index":8,"kind":"grass","seed":17}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":17,"method":"tools/call","params":{"name":"character_generate","arguments":{"character_id":"invalid_race","name":"Invalid Race","age_group":"adult","race":"dragon","seed":4}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":20,"method":"tools/call","params":{"name":"world_generate","arguments":{"map_id":"oversized_world","name":"Oversized World","width":2560,"height":1281,"seed":2718,"topology":"continents","water_level":50,"vegetation":65,"decay":15}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":21,"method":"tools/call","params":{"name":"biome_apply","arguments":{"map_id":"new_world","edits":[{"x":-1,"y":2,"biome_id":"coastal_mist"}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":22,"method":"tools/call","params":{"name":"biome_apply","arguments":{"map_id":"new_world","edits":[{"x":24,"y":2,"biome_id":"coastal_mist"}]}}})" << '\n';
    input << R"({"jsonrpc":"2.0","id":23,"method":"tools/call","params":{"name":"biome_apply","arguments":{"map_id":"new_world","edits":[{"x":0,"y":-1,"biome_id":"coastal_mist"}]}}})" << '\n';

    std::stringstream output;
    aur::run_mcp_server(project, input, output);

    std::vector<aur::Json> responses;
    std::string line;
    while (std::getline(output, line)) responses.push_back(aur::parse_json(line));
    ASSERT(responses.size() == 23);
    ASSERT(responses[0]["result"]["protocolVersion"].as_string() == "2024-11-05");
    ASSERT(responses[1]["result"]["tools"].as_array().size() >= 10);
    ASSERT(!responses[2]["result"]["isError"].as_bool());
    ASSERT(!responses[3]["result"]["isError"].as_bool());
    ASSERT(!responses[4]["result"]["isError"].as_bool());
    ASSERT(!responses[5]["result"]["isError"].as_bool());
    ASSERT(!responses[6]["result"]["isError"].as_bool());
    ASSERT(!responses[7]["result"]["isError"].as_bool());
    ASSERT(responses[8]["result"]["isError"].as_bool());
    ASSERT(!responses[9]["result"]["isError"].as_bool());
    ASSERT(!responses[10]["result"]["isError"].as_bool());
    ASSERT(!responses[11]["result"]["isError"].as_bool());
    ASSERT(!responses[12]["result"]["isError"].as_bool());
    ASSERT(!responses[13]["result"]["isError"].as_bool());
    ASSERT(!responses[14]["result"]["isError"].as_bool());
    ASSERT(!responses[15]["result"]["isError"].as_bool());
    ASSERT(!responses[16]["result"]["isError"].as_bool());
    ASSERT(!responses[17]["result"]["isError"].as_bool());
    ASSERT(responses[18]["result"]["isError"].as_bool());
    ASSERT(responses[19]["result"]["isError"].as_bool());
    ASSERT(!responses[20]["result"]["isError"].as_bool());
    ASSERT(!responses[21]["result"]["isError"].as_bool());
    ASSERT(responses[22]["result"]["isError"].as_bool());
    bool world_generation_schema_has_dimension_limits = false;
    for (const aur::Json& tool : responses[1]["result"]["tools"].as_array()) {
        if (tool["name"].as_string() != "world_generate") continue;
        const aur::Json& properties = tool["inputSchema"]["properties"];
        world_generation_schema_has_dimension_limits =
            properties["width"]["maximum"].as_int() == 2560 &&
            properties["height"]["maximum"].as_int() == 1280;
    }
    ASSERT(world_generation_schema_has_dimension_limits);
    ASSERT(project.maps["overworld"]["layers"]["ground"][0][1].as_int() == 0);
    ASSERT(project.maps["overworld"]["layers"]["biomes"][1][0].as_string() == "coastal_mist");
    ASSERT(project.tilesets["world"]["palettes"][0][1].as_string() == "#3f8f45");
    ASSERT(project.maps["new_world"]["layers"]["terrain_overrides"][1][1].as_int() == 2);
    ASSERT(project.maps["new_world"]["layers"]["biomes"][2][2].as_string() == "coastal_mist");
    ASSERT(project.maps["new_world"]["layers"]["biomes"][2][23].as_string() == "coastal_mist");
    ASSERT(project.maps["new_world"]["layers"]["biomes"][2][0].as_string() == "coastal_mist");

    const aur::Project saved = aur::Project::load(root.string());
    ASSERT(saved.characters.contains("ari"));
    ASSERT(saved.graphics.contains("hero"));
    ASSERT(saved.db.at("enemies").contains("moth"));
    ASSERT(saved.tilesets.at("world")["tiles"][0]["name"].as_string() == "grass-updated");
    ASSERT(saved.maps.at("overworld")["layers"]["ground"][0][1].as_int() == 0);
    ASSERT(saved.tilesets.at("world")["palettes"][0][1].as_string() == "#3f8f45");
    ASSERT(saved.biomes.contains("coastal_mist"));
    ASSERT(saved.maps.contains("new_world"));
    ASSERT(saved.maps.at("new_world")["worldgen"]["seed"].as_int() == 2718);
    ASSERT(saved.maps.at("new_world")["worldgen"]["topology"].as_string() == "archipelago");
    ASSERT(saved.maps.at("new_world")["worldgen"]["style"].as_string() == "hd2d");
    ASSERT(!saved.maps.at("new_world")["layers"].contains("ground"));
    ASSERT(saved.maps.at("new_world")["layers"]["terrain_overrides"][1][1].as_int() == 2);
    ASSERT(saved.maps.at("new_world")["layers"]["biomes"][2][23].as_string() == "coastal_mist");
    ASSERT(saved.maps.at("new_world")["layers"]["biomes"][2][0].as_string() == "coastal_mist");
    ASSERT(saved.characters.at("child_guide")["age_group"].as_string() == "child");
    ASSERT(saved.characters.at("child_guide")["race"].as_string() == "featherfolk");
    ASSERT(saved.characters.at("child_guide")["gender"].as_string() == "non-binary");
    ASSERT(saved.characters.at("child_guide")["presentation"].as_string() == "feminine");
    ASSERT(saved.graphics.at("child_guide")["spec"]["race"].as_string() == "featherfolk");
    ASSERT(saved.graphics.at("child_guide")["frames"].contains("down_left_0"));
    ASSERT(saved.db.at("enemies")["house_cat"]["pet"].as_bool());
    ASSERT(saved.graphics.at("healing_spark")["anims"].contains("play"));
    ASSERT(saved.tilesets.at("world")["tiles"].as_array().size() == 9);
    ASSERT(saved.tilesets.at("world")["tiles"][8]["name"].as_string() == "grass_17");
}
