#include "mini_test.hpp"
#include "aur/field.hpp"
#include "aur/project.hpp"
#include "aur/world_topology.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

    aur::Json Object(std::initializer_list<std::pair<const std::string, aur::Json>> values)
    {
        return aur::JsonObject(values);
    }

    void expect_rejected(aur::Project &project)
    {
        bool rejected = false;
        try
        {
            aur::paint_biome(project, "test_map", -1, 0, "plains");
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        ASSERT(rejected);
    }

    aur::Project make_paint_project(const std::string& width) {
        aur::Project project;
        project.tilesets["world"] = Object({{"tile", 16},
                                            {"tiles", aur::JsonArray{Object({{"flags", 0}})}},
                                            {"palettes", aur::JsonArray{aur::JsonArray{}}}});
        project.biomes["plains"] = Object({{"tileset", "world"},
                                           {"base_tile", 0},
                                           {"climate", "temperate"}});
        project.meta["weather"] = aur::JsonObject{{"profiles", aur::JsonObject{
            {"temperate", aur::JsonObject{}}}}};
        // width is written from raw JSON so malformed values reach the public boundary.
        aur::Json map = aur::parse_json(
            "{\"id\":\"test_map\",\"name\":\"Test Map\",\"kind\":\"field\","
            "\"w\":" +
            width + ",\"h\":2,\"tileset\":\"world\","
                    "\"layers\":{\"ground\":[[0,0],[0,0]]}}");
        map["layers"]["ground"] = aur::JsonArray{
            aur::JsonArray{0, 0}, aur::JsonArray{0, 0}};
        map["layers"]["biomes"] = aur::JsonArray{
            aur::JsonArray{"", ""}, aur::JsonArray{"", ""}};
        project.maps["test_map"] = map;
        return project;
    }

} // namespace

void test_topology_paint_boundaries()
{
    aur::Project bounded = make_paint_project("2");
    // A fully formed ordinary map does not acquire wrapping from its rectangular shape.
    ASSERT(!aur::wraps_world_x(bounded.maps.at("test_map")));
    expect_rejected(bounded);
    aur::paint_biome(bounded, "test_map", 1, 1, "plains");
    ASSERT(bounded.maps["test_map"]["layers"]["ground"][1][1].as_int() == 0);
    bool invalid_y_rejected = false;
    try
    {
        aur::paint_biome(bounded, "test_map", 0, 2, "plains");
    }
    catch (const std::runtime_error &)
    {
        invalid_y_rejected = true;
    }
    ASSERT(invalid_y_rejected);

    aur::Project wrapped = make_paint_project("2");
    wrapped.maps["test_map"]["worldgen"] = Object({{"topology", "continents"}});
    aur::paint_biome(wrapped, "test_map", -1, 0, "plains");
    ASSERT(wrapped.maps["test_map"]["layers"]["biomes"][0][1].as_string() == "plains");
    aur::paint_biome(wrapped, "test_map", 3, 1, "plains");
    ASSERT(wrapped.maps["test_map"]["layers"]["biomes"][1][1].as_string() == "plains");
    invalid_y_rejected = false;
    try
    {
        aur::paint_biome(wrapped, "test_map", -1, -1, "plains");
    }
    catch (const std::runtime_error &)
    {
        invalid_y_rejected = true;
    }
    ASSERT(invalid_y_rejected);

    aur::Project invalid_width = make_paint_project("0");
    invalid_width.maps["test_map"]["worldgen"] = Object({{"topology", "fractured"}});
    bool invalid_width_rejected = false;
    try
    {
        aur::paint_biome(invalid_width, "test_map", -1, 0, "plains");
    }
    catch (const std::invalid_argument &)
    {
        invalid_width_rejected = true;
    }
    ASSERT(invalid_width_rejected);
}

namespace {

std::string biome_climate_at(const aur::Json& map, double hero_x, double hero_y) {
    const int width = map["w"].as_int();
    const int height = map["h"].as_int();
    const int raw_x = static_cast<int>(std::floor(hero_x / aur::field_tile_size));
    const int x = aur::wraps_world_x(map) ? aur::wrapX(raw_x, width)
                                          : std::clamp(raw_x, 0, width - 1);
    const int y = std::clamp(static_cast<int>(std::floor(hero_y / aur::field_tile_size)), 0, height - 1);
    const auto& rows = map["layers"]["biomes"].as_array();
    if (y >= static_cast<int>(rows.size()) || !rows[static_cast<std::size_t>(y)].is_array() ||
        x >= static_cast<int>(rows[static_cast<std::size_t>(y)].as_array().size())) {
        return map["climate"].is_string() ? map["climate"].as_string() : "temperate";
    }
    std::string biome;
    try {
        biome = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)].as_string();
    } catch (const std::out_of_range&) {
        return map["climate"].is_string() ? map["climate"].as_string() : "temperate";
    }
    return biome == "arctic" ? "polar" : biome == "tropical" ? "tropical" : "temperate";
}

} // namespace

void test_topology_field_climate_and_camera()
{
    aur::Project project;
    project.tilesets["world"] = Object({{"tile", 16},
                                        {"tiles", aur::JsonArray{Object({{"flags", 0}})}}});

    auto make_field = [&](const char *topology, int width, int height)
    {
        aur::Json map = aur::new_map(
            "climate_field", width, height, "world", "field", "action", "hd2d", 0);
        map["worldgen"] = Object({{"topology", topology}});
        return map;
    };

    aur::Json map = make_field("continents", 10, 3);
    // Full-map-width rows keep every edge sample valid; only the seam cells differ.
    aur::JsonArray row_values(10, aur::Json("arctic"));
    row_values[9] = aur::Json("tropical");
    const aur::Json climate_row = row_values;
    map["layers"]["biomes"] = aur::JsonArray{climate_row, climate_row, climate_row};
    map["climate"] = "temperate";
    project.maps["climate_field"] = map;

    // Negative X wraps to the last column; the width is the map's own 10, not 4 or 256.
    ASSERT(biome_climate_at(map, -1.0, 0.5 * aur::field_tile_size) == "tropical");
    ASSERT(biome_climate_at(map, 9.0 * aur::field_tile_size, 0.5 * aur::field_tile_size) == "tropical");
    ASSERT(biome_climate_at(map, 10.0 * aur::field_tile_size + 1.0, 0.5 * aur::field_tile_size) == "polar");

    // A malformed (short) biome row must fall back to the map climate, not index OOB.
    map["layers"]["biomes"] = aur::JsonArray{
        aur::JsonArray{"arctic"}, aur::JsonArray{"arctic"}, aur::JsonArray{"arctic"}};
    ASSERT(biome_climate_at(map, 9.0 * aur::field_tile_size, 0.5 * aur::field_tile_size) == "temperate");
    ASSERT(aur::wraps_world_x(map));
    // An ordinary bounded map keeps the same sample bounded, never wrapped.
    map["worldgen"] = aur::JsonObject{};
    ASSERT(!aur::wraps_world_x(map));
    ASSERT(biome_climate_at(map, -1.0, 0.5 * aur::field_tile_size) == "polar");

    // Generated-world cameras repeat in X but stay inside the world in Y.
    aur::Game game(&project);
    game.layout.w = 64;
    game.layout.h = 64;
    aur::Json tall_map = make_field("archipelago", 10, 40);
    aur::FieldScene tall_field(&game, tall_map);
    tall_field.hero.x = -3.0 * aur::field_tile_size;
    tall_field.hero.y = 0.0;
    aur::InputState up;
    up.press("up");
    tall_field.update(0.5, up);
    ASSERT(tall_field.cam_y == 0.0);
    tall_field.hero.y = 40.0 * aur::field_tile_size;
    tall_field.update(0.0, up);
    const double expected_bottom = 40.0 * aur::field_tile_size - game.layout.h;
    ASSERT(tall_field.cam_y == expected_bottom);
    tall_field.hero.x = -3.0 * aur::field_tile_size;
    tall_field.update(0.0, up);
    ASSERT(tall_field.cam_x == tall_field.hero.x - game.layout.w / 2.0);
}
