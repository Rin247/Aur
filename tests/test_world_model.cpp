#include "mini_test.hpp"
#include "WorldModel.hpp"
#include "aur/world_topology.hpp"

#include <array>
#include <map>
#include <stdexcept>
#include <string>

void test_world_model_rect_edits() {
    using aur::editor::WorldModel;
    aur::Json map = aur::JsonObject{
        {"w", 3},
        {"h", 2},
        {"worldgen", aur::JsonObject{{"sea_level", 500}}},
        {"layers", aur::JsonObject{
            {"elevation", aur::JsonArray{
                aur::JsonArray{0, 250, 500},
                aur::JsonArray{750, 900, 1000}
            }},
            {"water", aur::JsonArray{
                aur::JsonArray{0, 0, 1},
                aur::JsonArray{2, 0, 0}
            }},
            {"biomes", aur::JsonArray{
                aur::JsonArray{"plains", "forest", "plains"},
                aur::JsonArray{"forest", "forest", "plains"}
            }}
        }}
    };
    const aur::JsonObject biomes{
        {"forest", aur::JsonObject{}},
        {"plains", aur::JsonObject{}}
    };
    WorldModel model = WorldModel::fromMap(map, biomes);
    const std::string originalLayerJson = aur::dump_json(map["layers"]);
    ASSERT(model.width() == 3);
    ASSERT(model.height() == 2);
    ASSERT(model.seaLevel() == 32768);
    ASSERT(model.heights()[0] == 0);
    ASSERT(model.heights()[2] == 32768);
    ASSERT(model.heights()[5] == 65535);
    ASSERT(model.biomeNames().size() == 2);
    ASSERT(model.waterTypes()[2] == 1);
    ASSERT(model.waterTypes()[3] == 2);

    const auto originalHeights = model.heights();
    const auto originalBiomes = model.biomes();
    const WorldModel::Rect area{1, 0, 2, 2};
    const auto beforeHeight = model.captureRect(WorldModel::Layer::Height, area);
    const auto beforeBiome = model.captureRect(WorldModel::Layer::Biome, area);
    const std::array<std::uint16_t, 4> heightEdit{12000, 24000, 36000, 48000};
    const std::array<std::uint8_t, 4> biomeEdit{0, 0, 1, 0};
    model.setRect(WorldModel::Layer::Height, area, heightEdit);
    model.setRect(WorldModel::Layer::Biome, area, biomeEdit);
    model.writeRect(map, WorldModel::Layer::Height, area);
    model.writeRect(map, WorldModel::Layer::Biome, area);
    ASSERT(map["layers"]["elevation"][0][1].as_int() == 183);
    ASSERT(map["layers"]["elevation"][1][2].as_int() == 732);
    ASSERT(map["layers"]["biomes"][0][1].as_string() == model.biomeNames()[0]);

    WorldModel::RectSnapshot redoHeight = model.captureRect(WorldModel::Layer::Height, area);
    WorldModel::RectSnapshot redoBiome = model.captureRect(WorldModel::Layer::Biome, area);
    model.restore(beforeHeight);
    model.restore(beforeBiome);
    model.writeRect(map, WorldModel::Layer::Height, area);
    model.writeRect(map, WorldModel::Layer::Biome, area);
    ASSERT(model.heights() == originalHeights);
    ASSERT(model.biomes() == originalBiomes);
    ASSERT(aur::dump_json(map["layers"]) == originalLayerJson);

    model.restore(redoHeight);
    model.restore(redoBiome);
    ASSERT(model.heights()[1] == heightEdit[0]);
    ASSERT(model.biomes()[1] == biomeEdit[0]);
    const auto dirty = model.takeDirtyRects();
    ASSERT(dirty.size() == 6);
    ASSERT(dirty.front().rect.width == area.width);
    ASSERT(dirty.front().rect.height == area.height);

    bool invalidRectRejected = false;
    try {
        model.captureRect(WorldModel::Layer::Height, {2, 1, 2, 1});
    } catch (const std::out_of_range&) {
        invalidRectRejected = true;
    }
    ASSERT(invalidRectRejected);
}

void test_world_model_wrap_edits() {
    using aur::editor::WorldModel;
    aur::Json map = aur::JsonObject{
        {"w", 4},
        {"h", 2},
        {"worldgen", aur::JsonObject{{"topology", "continents"}}},
        {"layers", aur::JsonObject{
            {"elevation", aur::JsonArray{
                aur::JsonArray{0, 250, 500, 750},
                aur::JsonArray{100, 300, 600, 900}
            }},
            {"water", aur::JsonArray{
                aur::JsonArray{0, 0, 0, 0},
                aur::JsonArray{0, 0, 0, 0}
            }},
            {"biomes", aur::JsonArray{
                aur::JsonArray{"plains", "forest", "plains", "forest"},
                aur::JsonArray{"forest", "plains", "forest", "plains"}
            }}
        }}
    };
    const WorldModel model_source = WorldModel::fromMap(
        map, {{"forest", aur::JsonObject{}}, {"plains", aur::JsonObject{}}});
    WorldModel model = model_source;
    ASSERT(model.wrapsX());
    const std::array<std::string, 4> generated_topologies{
        "continents", "archipelago", "supercontinent", "fractured"};
    for (const std::string& topology : generated_topologies) {
        const aur::Json world = aur::parse_json(
            std::string("{\"worldgen\":{\"topology\":\"") + topology + "\"}}");
        ASSERT(aur::wraps_world_x(world));
        ASSERT(aur::worldX(world, -1, 4) == 3);
    }
    ASSERT(aur::wraps_world_x(map));
    ASSERT(aur::worldX(map, -1, 4) == 3);
    ASSERT(aur::worldX(map, 4, 4) == 0);
    ASSERT(!aur::wraps_world_x(aur::JsonObject{{"w", 4}, {"h", 2}}));
    ASSERT(!aur::wraps_world_x(aur::JsonObject{{"worldgen", aur::JsonObject{}}}));
    const aur::Json non_string_topology = aur::parse_json(
        R"({"worldgen":{"topology":7}})");
    ASSERT(!aur::wraps_world_x(non_string_topology));
    const aur::Json unsupported_topology = aur::parse_json(
        R"({"worldgen":{"topology":"flat"}})");
    ASSERT(!aur::wraps_world_x(unsupported_topology));
    ASSERT(aur::worldX(aur::JsonObject{{"w", 4}}, -1, 4) == -1);
    ASSERT(aur::wrapX(-1, 4) == 3);
    ASSERT(aur::wrapX(5, 4) == 1);
    ASSERT(aur::wrapX(-9, 4) == 3);
    ASSERT(aur::wrapX(13, 4) == 1);
    ASSERT(aur::wrapX(-7, 1) == 0);
    ASSERT(aur::wrapX(12, 1) == 0);

    ASSERT(aur::wrapped_delta_x(0, 2, 4) == 2);
    ASSERT(aur::wrapped_delta_x(2, 0, 4) == -2);
    ASSERT(aur::wrapped_delta_x(0, 2, 5) == 2);
    ASSERT(aur::wrapped_delta_x(2, 0, 5) == -2);
    ASSERT(aur::wrapped_delta_x(0, 3, 4) == -1);
    ASSERT(aur::wrapped_delta_x(3, 0, 4) == 1);

    bool invalid_wrap_width_rejected = false;
    try {
        (void)aur::wrapX(0, 0);
    } catch (const std::invalid_argument&) {
        invalid_wrap_width_rejected = true;
    }
    ASSERT(invalid_wrap_width_rejected);

    bool invalid_delta_width_rejected = false;
    try {
        (void)aur::wrapped_delta_x(0, 0, 0);
    } catch (const std::invalid_argument&) {
        invalid_delta_width_rejected = true;
    }
    ASSERT(invalid_delta_width_rejected);

    const WorldModel::Rect seam{3, 0, 2, 1};
    const auto before = model.captureRect(WorldModel::Layer::Height, seam);
    const std::array<std::uint16_t, 2> edit{12000, 24000};
    model.setRect(WorldModel::Layer::Height, seam, edit);
    model.writeRect(map, WorldModel::Layer::Height, seam);
    ASSERT(model.heights()[3] == edit[0]);
    ASSERT(model.heights()[0] == edit[1]);
    ASSERT(map["layers"]["elevation"][0][3].as_int() == 183);
    ASSERT(map["layers"]["elevation"][0][0].as_int() == 366);
    const auto dirty = model.takeDirtyRects();
    ASSERT(dirty.size() == 2);
    ASSERT(dirty[0].rect.x == 3 && dirty[0].rect.width == 1);
    ASSERT(dirty[1].rect.x == 0 && dirty[1].rect.width == 1);

    model.restore(before);
    model.writeRect(map, WorldModel::Layer::Height, seam);
    ASSERT(model.heights()[3] == before.heights[0]);
    ASSERT(model.heights()[0] == before.heights[1]);
    ASSERT(map["layers"]["elevation"][0][3].as_int() == 750);
    ASSERT(map["layers"]["elevation"][0][0].as_int() == 0);
}
