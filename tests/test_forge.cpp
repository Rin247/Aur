#include "mini_test.hpp"
#include "aur/forge.hpp"
#include "aur/project.hpp"
#include "bestiary/BestiaryData.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

void test_forge_generators() {
    const aur::Json character_spec = aur::random_character_spec(12345, "Child Guide", "child");
    const aur::Json character = aur::compile_character(character_spec);
    ASSERT(character["w"].as_int() == 48);
    ASSERT(character["h"].as_int() == 64);
    ASSERT(character["frames"].as_object().size() == 24);
    ASSERT(character["frames"].contains("down_left_0"));
    ASSERT(character["frames"]["down_0"].as_array().size() == 64);
    ASSERT(character["frames"]["down_0"][0].as_string().size() == 48);
    ASSERT(character["palette"].as_array().size() <= 16);
    ASSERT(aur::dump_json(character["frames"]["down_0"]) !=
           aur::dump_json(character["frames"]["up_0"]));
    ASSERT(aur::dump_json(character["frames"]["down_0"]) !=
           aur::dump_json(character["frames"]["down_1"]));
    const std::string detailed_frame = aur::dump_json(character["frames"]["down_0"]);
    for (char palette_index : {'1', '2', '3', '4', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'F'}) {
        ASSERT(detailed_frame.find(palette_index) != std::string::npos);
    }
    ASSERT(character["anims"].contains("walk_up_right"));
    ASSERT(character["spec"]["gender"].as_string() == "unspecified");
    ASSERT(character["spec"]["parts"]["presentation"].as_string() == "androgynous");
    ASSERT(!character["spec"]["parts"].contains("weapon"));
    ASSERT(aur::dump_json(character) ==
           aur::dump_json(aur::compile_character(
               aur::random_character_spec(12345, "Child Guide", "child"))));
    aur::Json equipped_spec = character_spec;
    equipped_spec["parts"]["weapon"] = "sword";
    ASSERT(aur::dump_json(aur::compile_character(equipped_spec)) ==
           aur::dump_json(character));
    const aur::Json feminine = aur::compile_character(
        aur::random_character_spec(12345, "Guide", "adult", "female", "feminine"));
    const aur::Json masculine = aur::compile_character(
        aur::random_character_spec(12345, "Guide", "adult", "male", "masculine"));
    ASSERT(feminine["spec"]["gender"].as_string() == "female");
    ASSERT(masculine["spec"]["gender"].as_string() == "male");
    ASSERT(aur::dump_json(feminine["frames"]["down_0"]) !=
           aur::dump_json(masculine["frames"]["down_0"]));

    const auto race_options = aur::character_race_options();
    ASSERT(race_options.size() == 15);
    for (const auto& [race, label] : race_options) {
        ASSERT(!label.empty());
        const aur::Json spec = aur::random_character_spec(
            12345, "Traveler", "adult", "unspecified", "androgynous", race);
        const aur::Json graphic = aur::compile_character(spec);
        ASSERT(spec["race"].as_string() == race);
        ASSERT(spec["parts"]["race"].as_string() == race);
        ASSERT(graphic["spec"]["race"].as_string() == race);
        ASSERT(aur::dump_json(spec) ==
               aur::dump_json(aur::random_character_spec(
                   12345, "Traveler", "adult", "unspecified", "androgynous", race)));
        ASSERT(graphic["w"].as_int() == 48);
        ASSERT(graphic["h"].as_int() == 64);
        ASSERT(aur::is_supported_character_race(race));
    }
    ASSERT(!aur::is_supported_character_race("dragons"));
    ASSERT(!aur::is_supported_character_race("unknown"));
    const auto skin_tone_for = [](const std::string& race) {
        return aur::random_character_spec(
            12345, "Traveler", "adult", "unspecified", "androgynous", race)["colors"]["skin"].as_string();
    };
    const std::vector<std::string> sylvani_tones = {"#f2d0cb", "#e7b8ae", "#d9aa9c", "#c5a083"};
    const std::vector<std::string> solani_tones = {"#c5a080", "#a97a58", "#80583f", "#573a2d", "#321f1b"};
    const std::vector<std::string> gharuk_tones = {
        "#87915b", "#6f7d4a", "#9d8653", "#86533a", "#573a2d", "#321f1b"
    };
    ASSERT(std::find(sylvani_tones.begin(), sylvani_tones.end(), skin_tone_for("sylvani")) !=
           sylvani_tones.end());
    ASSERT(std::find(solani_tones.begin(), solani_tones.end(), skin_tone_for("solani")) !=
           solani_tones.end());
    ASSERT(std::find(gharuk_tones.begin(), gharuk_tones.end(), skin_tone_for("gharuk")) !=
           gharuk_tones.end());
    bool invalid_race_rejected = false;
    try {
        (void)aur::random_character_spec(
            1, "Invalid", "adult", "unspecified", "androgynous", "dragon");
    } catch (const std::invalid_argument&) {
        invalid_race_rejected = true;
    }
    ASSERT(invalid_race_rejected);

    aur::Json elf_spec = aur::random_character_spec(
        12345, "Elf Scout", "adult", "female", "feminine");
    elf_spec["parts"]["ears"] = "elf";
    const aur::Json elf = aur::compile_character(elf_spec);
    ASSERT(elf["frames"]["down_0"][13].as_string()[7] == '1');
    ASSERT(elf["frames"]["down_0"][13].as_string()[40] == '1');
    ASSERT(aur::dump_json(elf["frames"]["down_0"]) !=
           aur::dump_json(character["frames"]["down_0"]));
    aur::Json elf_child_spec = elf_spec;
    elf_child_spec["age_group"] = "child";
    elf_child_spec["parts"]["age_group"] = "child";
    const aur::Json elf_child = aur::compile_character(elf_child_spec);
    aur::Json elf_teen_spec = elf_spec;
    elf_teen_spec["age_group"] = "teen";
    elf_teen_spec["parts"]["age_group"] = "teen";
    const aur::Json elf_teen = aur::compile_character(elf_teen_spec);
    const auto contains_visible_eye = [](const aur::Json& frame) {
        for (const aur::Json& row : frame.as_array()) {
            if (row.as_string().find('C') != std::string::npos) return true;
        }
        return false;
    };
    ASSERT(contains_visible_eye(elf_child["frames"]["down_0"]));
    ASSERT(contains_visible_eye(elf_child["frames"]["down_right_0"]));
    ASSERT(contains_visible_eye(elf_teen["frames"]["down_0"]));
    ASSERT(contains_visible_eye(elf_teen["frames"]["down_right_0"]));
    ASSERT(contains_visible_eye(elf["frames"]["down_0"]));
    ASSERT(contains_visible_eye(elf["frames"]["down_right_0"]));
    const auto top_pixel = [](const aur::Json& frame) {
        for (size_t y = 0; y < frame.as_array().size(); ++y) {
            if (frame[y].as_string().find_first_not_of('0') != std::string::npos) {
                return static_cast<int>(y);
            }
        }
        return -1;
    };
    const auto bottom_pixel = [](const aur::Json& frame) {
        for (int y = static_cast<int>(frame.as_array().size()) - 1; y >= 0; --y) {
            if (frame[static_cast<size_t>(y)].as_string().find_first_not_of('0') != std::string::npos) {
                return y;
            }
        }
        return -1;
    };
    for (const std::string& direction : {"down", "down_right", "up", "right"}) {
        const std::string frame_name = direction + "_0";
        const int baseline = bottom_pixel(elf_child["frames"][frame_name]);
        ASSERT(baseline == bottom_pixel(elf_teen["frames"][frame_name]));
        ASSERT(baseline == bottom_pixel(elf["frames"][frame_name]));
    }
    ASSERT(top_pixel(elf_child["frames"]["down_0"]) >
           top_pixel(elf_teen["frames"]["down_0"]));
    ASSERT(top_pixel(elf_teen["frames"]["down_0"]) >
           top_pixel(elf["frames"]["down_0"]));
    ASSERT(elf_child["w"].as_int() == elf_teen["w"].as_int());

    const auto catalog = aur::character_race_catalog();
    ASSERT(catalog.size() == 15);
    int mortal_races = 0;
    int supernatural_races = 0;
    int affinity_races = 0;
    for (const aur::RaceInfo& info : catalog) {
        ASSERT(!info.id.empty());
        ASSERT(!info.label.empty());
        if (info.group == aur::RaceGroup::Mortal) ++mortal_races;
        else ++supernatural_races;
        if (info.uses_affinity) ++affinity_races;
    }
    ASSERT(mortal_races == 6);
    ASSERT(supernatural_races == 9);
    ASSERT(affinity_races == 7);
    ASSERT(aur::alignment_options().size() == 7);
    ASSERT(aur::affinity_options().size() == 8);
    ASSERT(aur::entity_role_options().size() == 3);
    ASSERT(aur::hair_style_options().size() == 3);
    ASSERT(aur::accessory_options().size() == 4);

    for (const std::string& direction : {"down", "down_right", "right", "up_right",
                                         "up", "up_left", "left", "down_left"}) {
        const std::string name = direction + "_0";
        ASSERT(character["frames"].contains(name));
        ASSERT(character["frames"][name].as_array().size() == 64);
        ASSERT(character["frames"][name][0].as_string().size() == 48);
    }
    const auto mirrored_rows = [](const aur::Json& frame) {
        aur::JsonArray rows;
        for (const aur::Json& row : frame.as_array()) {
            std::string line = row.as_string();
            std::reverse(line.begin(), line.end());
            rows.emplace_back(line);
        }
        return aur::Json(rows);
    };
    ASSERT(aur::dump_json(mirrored_rows(character["frames"]["right_0"])) ==
           aur::dump_json(character["frames"]["left_0"]));
    ASSERT(aur::dump_json(mirrored_rows(character["frames"]["down_right_0"])) ==
           aur::dump_json(character["frames"]["down_left_0"]));

    const auto frame_of = [](const aur::Json& spec) {
        return aur::dump_json(aur::compile_character(spec)["frames"]["down_0"]);
    };
    aur::Json wolf_spec = aur::random_character_spec(
        777, "Wolf", "adult", "unspecified", "androgynous", "beastkin",
        "none", "unaligned", "npc", "wolf");
    aur::Json cat_spec = wolf_spec;
    cat_spec["parts"]["variant"] = "cat";
    ASSERT(frame_of(wolf_spec) != frame_of(cat_spec));
    aur::Json villager_spec = elf_spec;
    villager_spec["parts"]["role"] = "villager";
    aur::Json mage_spec = elf_spec;
    mage_spec["parts"]["role"] = "mage";
    aur::Json guard_spec = elf_spec;
    guard_spec["parts"]["role"] = "guard";
    ASSERT(frame_of(mage_spec) != frame_of(guard_spec));
    ASSERT(frame_of(mage_spec) != frame_of(villager_spec));
    ASSERT(frame_of(guard_spec) != frame_of(villager_spec));
    aur::Json long_hair_spec = elf_spec;
    long_hair_spec["parts"]["hair_style"] = "long";
    ASSERT(frame_of(long_hair_spec) != frame_of(elf_spec));
    aur::Json scarf_spec = elf_spec;
    scarf_spec["parts"]["accessory"] = aur::JsonArray{"scarf"};
    ASSERT(frame_of(scarf_spec) != frame_of(elf_spec));
    aur::Json circlet_spec = elf_spec;
    circlet_spec["parts"]["accessory"] = aur::JsonArray{"circlet"};
    ASSERT(frame_of(circlet_spec) != frame_of(scarf_spec));

    const aur::Json cat = aur::generate_creature_graphic("cat", 27);
    ASSERT(cat["w"].as_int() == 24);
    ASSERT(cat["frames"].as_object().size() == 4);
    ASSERT(aur::dump_json(cat) == aur::dump_json(aur::generate_creature_graphic("cat", 27)));

    for (const std::string& kind : {"bandit", "bandit_archer", "bandit_boss"}) {
        const aur::Json enemy = aur::generate_enemy_graphic(kind, 21);
        ASSERT(enemy["w"].as_int() == 24);
        ASSERT(enemy["h"].as_int() == 24);
        ASSERT(enemy["palette"].as_array().size() == 16);
        ASSERT(enemy["frames"].as_object().size() == 4);
        ASSERT(enemy["anims"].contains("idle"));
        ASSERT(aur::dump_json(enemy) == aur::dump_json(aur::generate_enemy_graphic(kind, 21)));
        const auto [frame_w, frame_h, frame_data] = aur::frame_rgba(enemy, "idle_0");
        ASSERT(frame_w == 24 && frame_h == 24);
        ASSERT(frame_data.size() == static_cast<size_t>(frame_w * frame_h * 4));
        ASSERT(frame_data[static_cast<size_t>(3)] == 0);
    }
    ASSERT(aur::dump_json(aur::generate_enemy_graphic("bandit", 21)) !=
           aur::dump_json(aur::generate_enemy_graphic("bandit_boss", 21)));
    bool enemy_threw = false;
    try { aur::generate_enemy_graphic("dragon", 1); } catch (const std::exception&) { enemy_threw = true; }
    ASSERT(enemy_threw);

    // The Bestiary's portrait path reads the first frame of the record's artwork.
    aur::Project portrait_project;
    portrait_project.graphics["bandit"] = aur::generate_enemy_graphic("bandit", 21);
    int art_width = 0;
    int art_height = 0;
    std::vector<unsigned char> art_pixels;
    ASSERT(aur::bestiary::artwork(portrait_project, "bandit", art_width, art_height, art_pixels));
    ASSERT(art_width == 24 && art_height == 24);
    ASSERT(art_pixels.size() == static_cast<size_t>(art_width * art_height * 4));

    const aur::Json effect = aur::generate_effect_graphic("heal", 87);
    ASSERT(effect["w"].as_int() == 32);
    ASSERT(effect["frames"].as_object().size() == 6);
    ASSERT(effect["anims"].contains("play"));

    for (const std::string& kind : {"shirt", "tunic", "pants", "shorts", "skirt",
                                    "dress", "cloak", "boots", "hat"}) {
        const aur::Json clothing = aur::generate_clothing_graphic(kind, 43);
        ASSERT(clothing["w"].as_int() == 32);
        ASSERT(clothing["h"].as_int() == 32);
        ASSERT(clothing["palette"].as_array().size() == 16);
        ASSERT(clothing["frames"]["0"].as_array().size() == 32);
        ASSERT(clothing["frames"]["0"][0].as_string().size() == 32);
        ASSERT(aur::validate_graphic(clothing).empty());
        ASSERT(aur::dump_json(clothing) ==
               aur::dump_json(aur::generate_clothing_graphic(kind, 43)));
    }
    for (const std::string& kind : {"sword", "staff", "shield", "bow", "lantern",
                                    "satchel", "potion"}) {
        const aur::Json prop = aur::generate_prop_graphic(kind, 71);
        ASSERT(prop["w"].as_int() == 32);
        ASSERT(prop["h"].as_int() == 32);
        ASSERT(prop["palette"].as_array().size() == 16);
        ASSERT(prop["frames"]["0"].as_array().size() == 32);
        ASSERT(prop["frames"]["0"][0].as_string().size() == 32);
        ASSERT(aur::validate_graphic(prop).empty());
        ASSERT(aur::dump_json(prop) ==
               aur::dump_json(aur::generate_prop_graphic(kind, 71)));
    }

    const aur::Json grass = aur::generate_tile_asset("grass", 17);
    ASSERT(grass["rows"].as_array().size() == 16);
    ASSERT(grass["name"].as_string() == "grass_17");
    ASSERT(aur::dump_json(grass) == aur::dump_json(aur::generate_tile_asset("grass", 17)));
    const aur::JsonArray starter_terrain = aur::generate_starter_world_terrain_tiles(4);
    ASSERT(starter_terrain.size() == 21);
    ASSERT(starter_terrain[0]["name"].as_string() == "starter_grassland");
    ASSERT(starter_terrain[1]["name"].as_string() == "starter_sand_coast");
    ASSERT(starter_terrain[2]["name"].as_string() == "starter_rocky_coast");
    ASSERT(starter_terrain[3]["name"].as_string() == "starter_shallow_sea");
    ASSERT(starter_terrain[4]["name"].as_string() == "starter_deep_sea");
    ASSERT(starter_terrain[6]["name"].as_string() == "starter_scree");
    ASSERT(starter_terrain[7]["name"].as_string() == "starter_snowfield");
    const std::array<std::string, 12> biome_tiles{
        "biome_verdant_plains", "biome_old_growth_forest", "biome_tropical_jungle",
        "biome_desert_wastes", "biome_alpine_highlands", "biome_frostfall_tundra",
        "biome_mistfen_marsh", "biome_volcanic_badlands", "biome_coast_and_islands",
        "biome_ancient_ruins", "biome_crystal_reaches", "biome_sky_isles"
    };
    for (size_t index = 0; index < biome_tiles.size(); ++index) {
        ASSERT(starter_terrain[index + 9]["name"].as_string() == biome_tiles[index]);
    }
    for (const aur::Json& tile : starter_terrain) {
        ASSERT(tile["rows"].as_array().size() == 32);
        ASSERT(tile["rows"][0].as_string().size() == 32);
        ASSERT(tile["pal"].as_int() >= 0 && tile["pal"].as_int() < 4);
    }
    const aur::JsonArray one_palette_terrain = aur::generate_starter_world_terrain_tiles(1);
    for (const aur::Json& tile : one_palette_terrain) ASSERT(tile["pal"].as_int() == 0);
    ASSERT(aur::dump_json(starter_terrain) ==
           aur::dump_json(aur::generate_starter_world_terrain_tiles(4)));

    const aur::JsonArray beach_tiles = aur::generate_field_beach_tiles(0);
    ASSERT(beach_tiles.size() == 17);
    ASSERT(beach_tiles[0]["name"].as_string() == "starter_field_sand");
    ASSERT(beach_tiles[0]["pal"].as_int() == 0);
    ASSERT(beach_tiles[0]["flags"].as_int() == 0);
    ASSERT(beach_tiles[1]["name"].as_string() == "field_beach_0");
    ASSERT(beach_tiles[16]["name"].as_string() == "field_beach_15");
    for (const aur::Json& tile : beach_tiles) {
        ASSERT(tile["rows"].as_array().size() == 32);
        ASSERT(tile["rows"][0].as_string().size() == 32);
        ASSERT(tile["flags"].as_int() == 0);
    }
    ASSERT(beach_tiles[1]["rows"][0].as_string().substr(0, 2) == "66");
    ASSERT(beach_tiles[1]["rows"][2].as_string().substr(4, 2) == "88");
    ASSERT(aur::dump_json(beach_tiles[16]["rows"]) == aur::dump_json(beach_tiles[0]["rows"]));
    ASSERT(aur::dump_json(beach_tiles) == aur::dump_json(aur::generate_field_beach_tiles(0)));
    ASSERT(aur::dump_json(beach_tiles) != aur::dump_json(aur::generate_field_beach_tiles(1)));

    const aur::Json low_water = aur::generate_world_map(
        "low_water", "Low Water", 64, 32, 2718, "continents", 20, 60, 10);
    const aur::Json high_water = aur::generate_world_map(
        "high_water", "High Water", 64, 32, 2718, "continents", 80, 60, 10);
    int low_water_land = 0;
    int high_water_land = 0;
    int sparse_forest = 0;
    int dense_forest = 0;
    const aur::Json sparse = aur::generate_world_map(
        "sparse", "Sparse", 64, 32, 2718, "continents", 50, 0, 0);
    const aur::Json dense = aur::generate_world_map(
        "dense", "Dense", 64, 32, 2718, "continents", 50, 100, 0);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (low_water["layers"]["elevation"][static_cast<size_t>(y)][static_cast<size_t>(x)].as_int() >
                low_water["worldgen"]["sea_level"].as_int()) {
                ++low_water_land;
            }
            if (high_water["layers"]["elevation"][static_cast<size_t>(y)][static_cast<size_t>(x)].as_int() >
                high_water["worldgen"]["sea_level"].as_int()) {
                ++high_water_land;
            }
            const std::string sparse_biome =
                sparse["layers"]["biomes"][static_cast<size_t>(y)][static_cast<size_t>(x)].as_string();
            if (sparse_biome == "old_growth_forest" || sparse_biome == "tropical_jungle") {
                ++sparse_forest;
            }
            const std::string dense_biome =
                dense["layers"]["biomes"][static_cast<size_t>(y)][static_cast<size_t>(x)].as_string();
            if (dense_biome == "old_growth_forest" || dense_biome == "tropical_jungle") {
                ++dense_forest;
            }
        }
    }
    ASSERT(low_water_land > high_water_land);
    ASSERT(dense_forest > sparse_forest);
    ASSERT(high_water["worldgen"]["water_level"].as_int() == 80);

    const aur::Json archipelago = aur::generate_world_map(
        "archipelago", "Isles of Koru", 96, 48, 9127, "archipelago", 50, 65, 15);
    const aur::Json archipelago_repeat = aur::generate_world_map(
        "archipelago", "Isles of Koru", 96, 48, 9127, "archipelago", 50, 65, 15);
    const aur::Json supercontinent = aur::generate_world_map(
        "supercontinent", "Veyra", 96, 48, 9127, "supercontinent", 50, 65, 15);
    const aur::Json fractured = aur::generate_world_map(
        "fractured", "The Shattered Reach", 96, 48, 9127, "fractured", 50, 65, 15);
    ASSERT(archipelago["worldgen"]["topology"].as_string() == "archipelago");
    ASSERT(archipelago["worldgen"]["style"].as_string() == "hd2d");
    ASSERT(archipelago["layers"].contains("elevation"));
    ASSERT(archipelago["layers"].contains("moisture"));
    ASSERT(archipelago["layers"].contains("water"));
    ASSERT(archipelago["layers"].contains("terrain_overrides"));
    ASSERT(!archipelago["layers"].contains("ground"));
    ASSERT(!archipelago["layers"].contains("objects"));
    ASSERT(!archipelago["layers"].contains("above"));
    ASSERT(archipelago["entities"].as_array().empty());
    ASSERT(aur::dump_json(archipelago) == aur::dump_json(archipelago_repeat));
    ASSERT(aur::dump_json(archipelago["layers"]["elevation"]) !=
           aur::dump_json(supercontinent["layers"]["elevation"]));
    ASSERT(aur::dump_json(archipelago["layers"]["elevation"]) !=
           aur::dump_json(fractured["layers"]["elevation"]));
    int generated_rivers = 0;
    int generated_lakes = 0;
    bool connected_river_cells = false;
    for (const aur::Json& row : archipelago["layers"]["water"].as_array()) {
        for (const aur::Json& cell : row.as_array()) {
            if (cell.as_int() == 1) ++generated_rivers;
            if (cell.as_int() == 2) ++generated_lakes;
        }
    }
    for (int y = 1; y < 47 && !connected_river_cells; ++y) {
        for (int x = 0; x < 96 && !connected_river_cells; ++x) {
            if (archipelago["layers"]["water"][static_cast<size_t>(y)]
                    [static_cast<size_t>(x)].as_int() != 1) continue;
            for (int dy = -1; dy <= 1 && !connected_river_cells; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = (x + dx + 96) % 96;
                    const int ny = y + dy;
                    if (archipelago["layers"]["water"][static_cast<size_t>(ny)]
                            [static_cast<size_t>(nx)].as_int() == 1) {
                        connected_river_cells = true;
                        break;
                    }
                }
            }
        }
    }
    ASSERT(generated_rivers > 0);
    ASSERT(generated_lakes > 0);
    ASSERT(connected_river_cells);
    bool invalid_topology_rejected = false;
    try {
        (void)aur::generate_world_map(
            "invalid", "Invalid", 24, 16, 1, "earth", 50, 60, 10);
    } catch (const std::runtime_error&) {
        invalid_topology_rejected = true;
    }
    ASSERT(invalid_topology_rejected);
    bool oversized_world_rejected = false;
    try {
        (void)aur::generate_world_map(
            "oversized", "Oversized", 2560, 1281, 1, "continents", 50, 60, 10);
    } catch (const std::runtime_error&) {
        oversized_world_rejected = true;
    }
    ASSERT(oversized_world_rejected);

    const aur::Json large_world = aur::generate_world_map(
        "large_world", "Large World", 2560, 1280, 20250308, "continents", 50, 65, 15);
    ASSERT(large_world["w"].as_int() == 2560);
    ASSERT(large_world["h"].as_int() == 1280);
    ASSERT(large_world["layers"]["elevation"].as_array().size() == 1280);
    ASSERT(large_world["layers"]["elevation"][0].as_array().size() == 2560);
    ASSERT(large_world["layers"]["water"].as_array().size() == 1280);
    ASSERT(large_world["layers"]["water"][0].as_array().size() == 2560);
    ASSERT(large_world["entities"].as_array().empty());
    int interior_polar_land = 0;
    for (int x = 0; x < 2560; ++x) {
        const auto all_land_neighbors = [&large_world](int column, int row) {
            const int width = large_world["w"].as_int();
            const int height = large_world["h"].as_int();
            const int sea = large_world["worldgen"]["sea_level"].as_int();
            if (large_world["layers"]["elevation"][static_cast<size_t>(row)]
                    [static_cast<size_t>(column)].as_int() <= sea) return false;
            if (large_world["layers"]["elevation"][static_cast<size_t>(row)]
                    [static_cast<size_t>((column + width - 1) % width)].as_int() <= sea ||
                large_world["layers"]["elevation"][static_cast<size_t>(row)]
                    [static_cast<size_t>((column + 1) % width)].as_int() <= sea) return false;
            if (row > 0 && large_world["layers"]["elevation"][static_cast<size_t>(row - 1)]
                    [static_cast<size_t>(column)].as_int() <= sea) return false;
            if (row + 1 < height && large_world["layers"]["elevation"][static_cast<size_t>(row + 1)]
                    [static_cast<size_t>(column)].as_int() <= sea) return false;
            return true;
        };
        if (all_land_neighbors(x, 0)) {
            ++interior_polar_land;
            ASSERT(large_world["layers"]["biomes"][0][static_cast<size_t>(x)].as_string() !=
                   "coast_and_islands");
        }
        if (all_land_neighbors(x, 1279)) {
            ++interior_polar_land;
            ASSERT(large_world["layers"]["biomes"][1279][static_cast<size_t>(x)].as_string() !=
                   "coast_and_islands");
        }
    }
    ASSERT(interior_polar_land > 0);

    const aur::Json tilted_world = aur::generate_world_map(
        "tilted_world", "Tilted World", 64, 32, 2718, "continents", 50, 60, 10, 90);
    ASSERT(tilted_world["worldgen"]["axial_tilt"].as_int() == 90);
    ASSERT(tilted_world["layers"].contains("temperature"));
    ASSERT(tilted_world["layers"]["temperature"].as_array().size() == 32);
    ASSERT(tilted_world["layers"]["temperature"][0].as_array().size() == 64);
    const aur::Json tilted_repeat = aur::generate_world_map(
        "tilted_world", "Tilted World", 64, 32, 2718, "continents", 50, 60, 10, 90);
    ASSERT(aur::dump_json(tilted_world) == aur::dump_json(tilted_repeat));
    for (const aur::Json& temperature_row :
         tilted_world["layers"]["temperature"].as_array()) {
        int latitude_temperature = -1;
        for (const aur::Json& temperature : temperature_row.as_array()) {
            if (temperature.as_int() < 0) continue;
            if (latitude_temperature < 0) latitude_temperature = temperature.as_int();
            ASSERT(temperature.as_int() == latitude_temperature);
        }
    }

    aur::Project project;
    aur::JsonArray collision_tiles(8, aur::JsonObject{{"flags", 0}});
    collision_tiles[0]["flags"] = aur::SOLID;
    project.tilesets["world"] = aur::JsonObject{{"tiles", collision_tiles}};
    project.maps["earth"] = low_water;
    aur::paint_biome(project, "earth", 1, 1, "verdant_plains");
    ASSERT(project.maps["earth"]["layers"]["biomes"][1][1].as_string() == "verdant_plains");
    aur::Json hd_world = archipelago;
    project.maps["hd_world"] = hd_world;
    aur::paint_biome(project, "hd_world", 1, 1, "verdant_plains");
    ASSERT(project.maps["hd_world"]["layers"]["biomes"][1][1].as_string() == "verdant_plains");
    const auto hd_collision = project.collision(project.maps["hd_world"]);
    ASSERT(hd_collision.size() == 48);
    ASSERT(hd_collision[0].size() == 96);

    aur::Json bounded_map = aur::new_map(
        "bounded", 8, 4, "world", "world", "action", "hd2d", 0);
    project.maps["bounded"] = bounded_map;
    aur::paint_biome(project, "bounded", 2, 1, "verdant_plains");
    ASSERT(project.maps["bounded"]["layers"]["biomes"][1][2].as_string() == "verdant_plains");
    for (const auto [x, y] : std::array<std::pair<int, int>, 4>{{{-1, 1}, {8, 1}, {2, -1}, {2, 4}}}) {
        bool rejected = false;
        try {
            aur::paint_biome(project, "bounded", x, y, "verdant_plains");
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        ASSERT(rejected);
    }
    aur::paint_biome(project, "hd_world", -1, 1, "old_growth_forest");
    ASSERT(project.maps["hd_world"]["layers"]["biomes"][1][95].as_string() == "old_growth_forest");

    aur::paint_biome(project, "hd_world", 96, 1, "tropical_jungle");
    ASSERT(project.maps["hd_world"]["layers"]["biomes"][1][0].as_string() == "tropical_jungle");
    ASSERT(project.maps["hd_world"]["layers"]["biomes"][1][95].as_string() == "old_growth_forest");
    for (const auto [x, y] : std::array<std::pair<int, int>, 2>{{{0, -1}, {0, 48}}}) {
        bool rejected = false;
        try {
            aur::paint_biome(project, "hd_world", x, y, "verdant_plains");
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        ASSERT(rejected);
    }
}
