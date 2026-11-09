#pragma once

#include "json.hpp"
#include <string>
#include <map>
#include <vector>
#include <atomic>

namespace aur {

constexpr int SOLID = 1;
constexpr int WATER = 2;
constexpr int SLOW = 4;
constexpr int HURT = 8;
constexpr int ENCOUNTER = 16;
constexpr int BRIDGE = 32;

class Project {
public:
    std::string root;
    JsonObject meta;
    JsonObject tilesets;
    JsonObject graphics;
    JsonObject characters;
    JsonObject db;
    JsonObject events;
    JsonObject parts;
    JsonObject biomes;
    JsonObject maps;
    JsonObject music;

    mutable JsonObject compiled_cache;

    explicit Project(std::string root = ".") : root(std::move(root)) {
        db["actors"] = JsonObject{};
        db["classes"] = JsonObject{};
        db["races"] = JsonObject{};
        db["enemies"] = JsonObject{};
        db["items"] = JsonObject{};
        db["weapons"] = JsonObject{};
        db["skills"] = JsonObject{};
        meta["title"] = "Untitled";
        JsonObject start_spawn;
        start_spawn["map"] = "";
        start_spawn["spawn"] = "start";
        meta["start"] = start_spawn;
        meta["clock_scale"] = 0.5;
        meta["start_hour"] = 8;
        meta["calendar"] = JsonObject{
            {"hours_per_day", 24},
            {"start_hour", 8},
            {"real_day_minutes", 48},
            {"days_per_season", 28},
            {"seasons", JsonArray{"spring", "summer", "autumn", "winter"}},
            {"phase_hours", JsonArray{6, 12, 18}}
        };
        JsonObject eras;
        auto add_era = [&eras](const std::string& id, const std::string& name,
                               const std::string& aesthetic, const std::string& conflict) {
            eras[id] = JsonObject{{"name", name}, {"aesthetic", aesthetic}, {"conflict", conflict}};
        };
        add_era("primordial_dawn", "The Primordial Dawn",
                "Overgrown sci-fi ruins, Seed Ship wreckage, and the first discoveries of magic.",
                "Survival; humans and native peoples meet.");
        add_era("dark_age", "The Dark Age",
                "War-torn fortifications beneath ash skies.",
                "The Vorashi surge and the Sylvani Aetheric Implosion.");
        add_era("middle_stability", "The Middle Stability",
                "Classic fantasy, established trade routes, and normalized magic.",
                "Political intrigue and Solani expansion.");
        add_era("industrial_weave", "The Industrial Weave",
                "Steampunk machinery fused with magic and runic technology.",
                "Technology, tradition, and competing powers.");
        add_era("severed_age", "The Severed Age",
                "Mana-dead neon arcologies and biopunk technology.",
                "Corporate extraction, identity, and social inequality.");
        meta["eras"] = eras;
        meta["current_era"] = "primordial_dawn";
        meta["weather"] = JsonObject{
            {"interval_hours", 3},
            {"profiles", JsonObject{
                {"temperate", JsonObject{{"clear", 40}, {"cloudy", 30}, {"rain", 25}, {"storm", 5}}},
                {"tropical", JsonObject{{"clear", 25}, {"cloudy", 25}, {"rain", 35}, {"storm", 15}}},
                {"arid", JsonObject{{"clear", 75}, {"cloudy", 20}, {"rain", 5}}},
                {"arctic", JsonObject{{"clear", 20}, {"cloudy", 25}, {"snow", 50}, {"storm", 5}}},
                {"none", JsonObject{{"clear", 100}}}
            }}
        };
        meta["party"] = JsonArray{};
        auto add_biome = [this](const std::string& id, const std::string& name,
                                const std::string& climate, int base_tile,
                                const std::string& color, const std::string& description) {
            Json biome;
            biome["name"] = name;
            biome["climate"] = climate;
            biome["tileset"] = "world";
            biome["base_tile"] = base_tile;
            biome["color"] = color;
            biome["description"] = description;
            biome["vegetation"] = 65;
            biome["decay"] = 10;
            biome["water_level"] = 50;
            biomes[id] = biome;
        };
        add_biome("verdant_plains", "Verdant Plains", "temperate", 0, "#75b96b",
                  "Open grasslands, farms, and rolling meadows.");
        add_biome("old_growth_forest", "Old-growth Forest", "temperate", 3, "#3b8054",
                  "Dense woodland and ancient forests.");
        add_biome("tropical_jungle", "Tropical Jungle", "temperate", 3, "#4c9a58",
                  "Warm rainforests, vines, and overgrown ruins.");
        add_biome("desert_wastes", "Desert Wastes", "arid", 6, "#d5ad68",
                  "Dunes, dry basins, and sun-baked badlands.");
        add_biome("alpine_highlands", "Alpine Highlands", "arctic", 4, "#96a6a8",
                  "High peaks, rocky passes, and thin-air plateaus.");
        add_biome("frostfall_tundra", "Frostfall Tundra", "arctic", 4, "#b8cbd2",
                  "Snowfields, frozen lakes, and northern wastes.");
        add_biome("mistfen_marsh", "Mistfen Marsh", "temperate", 3, "#5b8b78",
                  "Wetlands, reed beds, and fog-bound fens.");
        add_biome("volcanic_badlands", "Volcanic Badlands", "arid", 4, "#bb6654",
                  "Ash plains, lava country, and scorched stone.");
        add_biome("coast_and_islands", "Coast & Islands", "temperate", 6, "#56a2ad",
                  "Shorelines, reefs, and island chains.");
        add_biome("ancient_ruins", "Ancient Ruins", "temperate", 5, "#a58b73",
                  "Lost roads, overgrown monuments, and buried cities.");
        add_biome("crystal_reaches", "Crystal Reaches", "temperate", 5, "#9a84c5",
                  "Arcane mineral fields and luminous caverns.");
        add_biome("sky_isles", "Sky Isles", "temperate", 4, "#88b6d2",
                  "Floating landmasses and high-altitude sanctuaries.");
    }

    static Project load(const std::string& root_dir);
    void save(const std::string& root_dir = "");

    Json graphic(const std::string& gid) const;
    void invalidate(const std::string& gid = "");
    int tile_flags(const std::string& ts_id, int idx) const;
    std::vector<std::vector<int>> collision(const Json& m) const;
    std::vector<std::string> validate() const;
};

Json new_map(const std::string& mid, int w, int h, const std::string& tileset, const std::string& kind = "town", const std::string& mode = "action", const std::string& style = "snes", int fill = 0);
void paint_biome(Project& project, const std::string& map_id, int x, int y, const std::string& biome_id);
Json generate_world_map(const std::string& map_id, const std::string& name,
                        int width, int height, int seed, const std::string& topology = "continents",
                        int water_level = 50, int vegetation = 60, int decay = 10,
                        int axial_tilt = 0,
                        const std::atomic_bool* cancelled = nullptr);

} // namespace aur
