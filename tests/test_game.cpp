#include "mini_test.hpp"
#include "aur/field.hpp"

#include <algorithm>

void test_game_updates_field_scene() {
    aur::Project project;
    aur::Json field_map =
        aur::new_map("field", 32, 24, "world", "town", "action", "hd2d", 0);
    aur::JsonObject& layers = field_map["layers"].as_object();
    layers.erase("ground");
    layers.erase("objects");
    layers.erase("above");
    project.maps["field"] = field_map;
    project.meta["start"]["map"] = "field";

    aur::Game game(&project);
    game.layout.w = 640;
    game.layout.h = 360;
    aur::InputState input;
    game.update(0.016, input);

    ASSERT(game.scenes.size() == 1);
    auto* field = dynamic_cast<aur::FieldScene*>(game.scenes.front().get());
    ASSERT(field != nullptr);
    ASSERT(field->hero.x == 16.5 * aur::field_tile_size);
    ASSERT(field->hero.y == 13 * aur::field_tile_size - 1);
    ASSERT(field->hero.dir == "down");
    ASSERT(std::abs(field->cam_x - 208.0) < 0.001);
    ASSERT(std::abs(field->cam_y - 211.0) < 0.001);
    const double starting_x = field->hero.x;
    const double starting_camera_x = field->cam_x;
    input.press("right");
    game.update(0.1, input);

    ASSERT(field->hero.x > starting_x);
    ASSERT(field->cam_x > starting_camera_x);

    aur::NullRenderer renderer;
    renderer.begin("hd2d");
    ASSERT(renderer.w == 640);
    ASSERT(renderer.h == 360);
    field->draw(renderer);

    aur::Json topology_map = aur::new_map(
        "wrapped_field", 8, 8, "world", "world", "action", "hd2d", -1);
    topology_map["worldgen"] = aur::JsonObject{{"topology", "continents"}};
    topology_map["layers"]["ground"][4][7] = 0;
    topology_map["layers"]["ground"][4][0] = 0;
    project.tilesets["world"]["tiles"] = aur::JsonArray{
        aur::JsonObject{{"flags", aur::SOLID}}
    };
    project.maps["wrapped_field"] = topology_map;
    auto wrapped = std::make_unique<aur::FieldScene>(&game, topology_map);
    wrapped->hero.x = 7.5 * aur::field_tile_size;
    wrapped->hero.y = 4.5 * aur::field_tile_size;
    wrapped->flags[4][7] = aur::SOLID;
    wrapped->flags[4][0] = aur::SOLID;
    ASSERT(wrapped->tile_flags(wrapped->hero.x + 20.0, wrapped->hero.y) & aur::SOLID);
    ASSERT(wrapped->tile_flags(-20.0, wrapped->hero.y) & aur::SOLID);
    ASSERT(wrapped->tile_flags(wrapped->hero.x, -1.0) & aur::SOLID);
    ASSERT(wrapped->tile_flags(wrapped->hero.x, 8.0 * aur::field_tile_size) & aur::SOLID);
    wrapped->flags[4][7] = 0;
    wrapped->flags[4][0] = 0;

    aur::InputState wrapped_input;
    wrapped_input.press("right");
    wrapped->update(0.2, wrapped_input);
    ASSERT(wrapped->hero.x > 8.0 * aur::field_tile_size);
    ASSERT(wrapped->cam_x > wrapped->hero.x - game.layout.w / 2.0 - 1.0);
    renderer.calls.clear();
    wrapped->draw(renderer);
    ASSERT(renderer.count("tile") > 0);

    aur::Json polar_map = aur::new_map(
        "polar_field", 8, 24, "world", "world", "action", "hd2d", 0);
    polar_map["worldgen"] = aur::JsonObject{{"topology", "fractured"}};
    aur::FieldScene polar_field(&game, polar_map);
    polar_field.hero.y = 0.5 * aur::field_tile_size;
    aur::InputState up_input;
    up_input.press("up");
    polar_field.update(1.0, up_input);
    ASSERT(polar_field.hero.y == 0.5 * aur::field_tile_size);

    aur::Json bounded_map = aur::new_map(
        "bounded_field", 8, 8, "world", "world", "action", "hd2d", 0);
    aur::FieldScene bounded(&game, bounded_map);
    ASSERT(bounded.tile_flags(-1.0, 4.5 * aur::field_tile_size) & aur::SOLID);
    ASSERT(bounded.tile_flags(8.0 * aur::field_tile_size, 4.5 * aur::field_tile_size) & aur::SOLID);

    aur::Json wide_map = aur::new_map(
        "wide_world", 10, 4, "world", "world", "action", "hd2d", -1);
    wide_map["layers"]["ground"][2] = aur::JsonArray{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    wide_map["worldgen"] = aur::JsonObject{{"topology", "archipelago"}};
    aur::FieldScene wide_field(&game, wide_map);
    wide_field.hero.x = 8.5 * aur::field_tile_size;
    wide_field.hero.y = 2.5 * aur::field_tile_size;
    wide_field.cam_x = wide_field.hero.x - game.layout.w / 2.0;
    wide_field.cam_y = 0;
    wide_field.flags[2] = std::vector<int>(10, 0);
    struct TileRecorder final : aur::NullRenderer {
        std::vector<std::tuple<int, int, std::string>> tiles;
        void tile(const std::string&, int, int x, int y, const std::string& layer) override {
            tiles.emplace_back(x, y, layer);
            NullRenderer::tile("", 0, x, y, layer);
        }
    } tile_recorder;
    tile_recorder.begin("hd2d");
    wide_field.draw(tile_recorder);
    std::vector<int> ground_x;
    for (const auto& [x, y, layer] : tile_recorder.tiles) {
        if (y != 2 * aur::field_tile_size || layer != "ground") continue;
        ground_x.push_back(x);
    }
    ASSERT(std::find(ground_x.begin(), ground_x.end(), -64) != ground_x.end());
    ASSERT(std::find(ground_x.begin(), ground_x.end(), 0) != ground_x.end());
    ASSERT(std::find(ground_x.begin(), ground_x.end(), 8 * aur::field_tile_size) != ground_x.end());
}
