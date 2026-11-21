#include "project.hpp"
#include "world_topology.hpp"
#include "gfx.hpp"
#include "forge.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace aur {

static Json read_json_file(const std::string& path, const Json& default_val) {
    if (fs::exists(path)) {
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            try {
                return parse_json(ss.str());
            } catch (...) {
                return default_val;
            }
        }
    }
    return default_val;
}

static void write_json_file(const std::string& path, const Json& obj) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::out | std::ios::binary);
    if (!f) throw std::runtime_error("Unable to write project file: " + path);
    const std::string contents = dump_json(obj, 1, 0);
    f.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!f) throw std::runtime_error("Failed while writing project file: " + path);
}

Project Project::load(const std::string& root_dir) {
    Project p(root_dir);
    const JsonObject default_meta = p.meta;
    auto j_path = [&](const std::string& sub) {
        return (fs::path(root_dir) / sub).string();
    };

    const Json loaded_meta = read_json_file(j_path("project.json"), p.meta);
    const bool has_calendar_start = loaded_meta["calendar"]["start_hour"].is_number();
    const bool has_calendar_day_length = loaded_meta["calendar"]["real_day_minutes"].is_number();
    p.meta = loaded_meta.as_object();
    for (const auto& [key, value] : default_meta) {
        if (!p.meta.contains(key)) p.meta[key] = value;
    }
    for (const std::string key : {"calendar", "weather"}) {
        Json& settings = p.meta[key];
        const Json& defaults = default_meta.at(key);
        if (!settings.is_object()) settings = defaults;
        for (const auto& [setting, value] : defaults.as_object()) {
            if (!settings.contains(setting)) settings[setting] = value;
        }
    }
    if (!has_calendar_start && p.meta["start_hour"].is_number()) {
        p.meta["calendar"]["start_hour"] = p.meta["start_hour"];
    }
    if (!has_calendar_day_length && loaded_meta["clock_scale"].is_number() &&
        loaded_meta["clock_scale"].as_number() > 0.0) {
        const double hours = p.meta["calendar"]["hours_per_day"].as_number();
        p.meta["calendar"]["real_day_minutes"] = hours / loaded_meta["clock_scale"].as_number();
    }
    p.tilesets = read_json_file(j_path("tilesets.json"), JsonObject{}).as_object();
    if (!p.tilesets.contains("terra") && p.tilesets.contains("world") &&
        p.tilesets["world"].is_object()) {
        p.tilesets["terra"] = p.tilesets["world"];
    }
    if (p.tilesets.contains("terra") && p.tilesets["terra"].is_object()) {
        Json& field_tileset = p.tilesets["terra"];
        if (field_tileset["tiles"].is_array() && field_tileset["palettes"].is_array() &&
            !field_tileset["palettes"].as_array().empty() &&
            field_tileset["autotiles"].is_object() &&
            field_tileset["autotiles"].contains("water")) {
            const Json& water_group = field_tileset["autotiles"]["water"];
            const Json& water_variants = water_group["tiles"];
            const bool has_beach = field_tileset["autotiles"].contains("field_beach");
            if (!has_beach && water_group.is_object() && water_group.contains("tiles") &&
                water_variants.is_array() && water_variants.as_array().size() == 16) {
                JsonArray beach_tiles = generate_field_beach_tiles(0);
                JsonArray& tiles = field_tileset["tiles"].as_array();
                tiles.reserve(tiles.size() + beach_tiles.size());
                const int first = static_cast<int>(tiles.size());
                tiles.insert(tiles.end(), beach_tiles.begin(), beach_tiles.end());
                JsonArray variants;
                for (int mask = 0; mask < 16; ++mask) variants.emplace_back(first + 1 + mask);
                field_tileset["autotiles"]["field_beach"] = JsonObject{
                    {"edge_joined", true}, {"joins", JsonArray{Json("field_beach")}},
                    {"name", "field_beach"}, {"tiles", variants}
                };
            }
        }
    }
    p.graphics = read_json_file(j_path("graphics.json"), JsonObject{}).as_object();
    p.characters = read_json_file(j_path("characters.json"), JsonObject{}).as_object();
    p.db = read_json_file(j_path("database.json"), p.db).as_object();
    if (!p.db.contains("races") || !p.db["races"].is_object()) {
        p.db["races"] = JsonObject{};
    }
    p.events = read_json_file(j_path("events.json"), JsonObject{}).as_object();
    p.parts = read_json_file(j_path("parts.json"), JsonObject{}).as_object();
    const Json loaded_biomes = read_json_file(j_path("biomes.json"), Json(p.biomes));
    if (loaded_biomes.is_object()) {
        for (const auto& [id, biome] : loaded_biomes.as_object()) p.biomes[id] = biome;
    }

    std::string maps_dir = j_path("maps");
    if (fs::is_directory(maps_dir)) {
        for (const auto& entry : fs::directory_iterator(maps_dir)) {
            if (entry.path().extension() == ".json") {
                std::string name = entry.path().stem().string();
                p.maps[name] = read_json_file(entry.path().string(), JsonObject{});
            }
        }
    }

    std::string music_dir = j_path("music");
    if (fs::is_directory(music_dir)) {
        for (const auto& entry : fs::directory_iterator(music_dir)) {
            if (entry.path().extension() == ".json") {
                std::string name = entry.path().stem().string();
                p.music[name] = read_json_file(entry.path().string(), JsonObject{});
            }
        }
    }

    return p;
}

void Project::save(const std::string& root_dir) {
    std::string r = root_dir.empty() ? root : root_dir;
    auto j_path = [&](const std::string& sub) {
        return (fs::path(r) / sub).string();
    };

    write_json_file(j_path("project.json"), meta);
    write_json_file(j_path("tilesets.json"), tilesets);
    write_json_file(j_path("graphics.json"), graphics);
    write_json_file(j_path("characters.json"), characters);
    write_json_file(j_path("database.json"), db);
    write_json_file(j_path("events.json"), events);
    write_json_file(j_path("biomes.json"), biomes);
    if (!parts.empty()) {
        write_json_file(j_path("parts.json"), parts);
    }

    std::string maps_dir = j_path("maps");
    fs::create_directories(maps_dir);
    for (const auto& [k, v] : maps) {
        write_json_file((fs::path(maps_dir) / (k + ".json")).string(), v);
    }

    std::string music_dir = j_path("music");
    fs::create_directories(music_dir);
    for (const auto& [k, v] : music) {
        write_json_file((fs::path(music_dir) / (k + ".json")).string(), v);
    }
    root = r;
}

void paint_biome(Project& project, const std::string& map_id, int x, int y,
                 const std::string& biome_id) {
    auto map_it = project.maps.find(map_id);
    if (map_it == project.maps.end()) throw std::runtime_error("Map not found: " + map_id);
    auto biome_it = project.biomes.find(biome_id);
    if (biome_it == project.biomes.end()) throw std::runtime_error("Biome not found: " + biome_id);

    Json& map = map_it->second;
    const int width = map["w"].as_int();
    const int height = map["h"].as_int();
    if (y < 0 || y >= height ||
        (!wraps_world_x(map) && (x < 0 || x >= width)) ||
        !map.contains("layers") || !map["layers"].is_object()) {
        throw std::runtime_error("Biome paint position or map layer is invalid.");
    }
    if (wraps_world_x(map)) x = wrapX(x, width);
    Json& layers = map["layers"];
    const bool hd_surface = map.contains("worldgen") &&
        map["worldgen"].is_object() && map["worldgen"].contains("style") &&
        map["worldgen"]["style"].as_string() == "hd2d" &&
        layers.contains("elevation") && layers["elevation"].is_array();
    const bool has_ground = layers.contains("ground") && layers["ground"].is_array();
    if (!hd_surface && !has_ground) {
        throw std::runtime_error("Biome paint position or map layer is invalid.");
    }

    const std::string tileset_id = map["tileset"].as_string();
    const Json& biome = biome_it->second;
    if (biome["tileset"].as_string() != tileset_id) {
        throw std::runtime_error("Biome tileset does not match map tileset.");
    }
    const auto tileset_it = project.tilesets.find(tileset_id);
    if (tileset_it == project.tilesets.end() ||
        !tileset_it->second.contains("tiles") || !biome["base_tile"].is_number()) {
        throw std::runtime_error("Biome tileset or base tile is missing.");
    }
    if (!std::isfinite(biome["base_tile"].as_number()) ||
        biome["base_tile"].as_number() < 0 ||
        biome["base_tile"].as_number() >= static_cast<double>(tileset_it->second["tiles"].as_array().size())) {
        throw std::runtime_error("Biome base tile is outside the tileset range.");
    }
    const int base_tile = biome["base_tile"].as_int();

    if (!layers.contains("biomes")) {
        JsonArray rows;
        for (int row = 0; row < height; ++row) {
            rows.emplace_back(JsonArray(static_cast<size_t>(width), Json("")));
        }
        layers["biomes"] = rows;
    }
    Json& biome_layer = layers["biomes"];
    if (!biome_layer.is_array() || static_cast<int>(biome_layer.as_array().size()) != height) {
        throw std::runtime_error("Biome layer dimensions do not match map metadata.");
    }
    Json& biome_row = biome_layer[static_cast<size_t>(y)];
    if (!biome_row.is_array() || static_cast<int>(biome_row.as_array().size()) != width) {
        throw std::runtime_error("Biome layer dimensions do not match map metadata.");
    }
    Json* ground_row = nullptr;
    if (has_ground) {
        if (static_cast<int>(layers["ground"].as_array().size()) != height) {
            if (!hd_surface) {
                throw std::runtime_error("Ground layer dimensions do not match map metadata.");
            }
        } else {
            Json& row = layers["ground"][static_cast<size_t>(y)];
            if (row.is_array() && static_cast<int>(row.as_array().size()) == width) {
                ground_row = &row;
            } else if (!hd_surface) {
                throw std::runtime_error("Ground layer dimensions do not match map metadata.");
            }
        }
    }

    biome_row[static_cast<size_t>(x)] = biome_id;
    if (ground_row) (*ground_row)[static_cast<size_t>(x)] = base_tile;
}

Json Project::graphic(const std::string& gid) const {
    auto it = compiled_cache.find(gid);
    if (it != compiled_cache.end()) return it->second;

    Json g;
    auto g_it = graphics.find(gid);
    if (g_it != graphics.end()) {
        g = g_it->second;
    } else {
        g = new_graphic();
    }
    compiled_cache[gid] = g;
    return g;
}

void Project::invalidate(const std::string& gid) {
    if (gid.empty()) {
        compiled_cache.clear();
    } else {
        compiled_cache.erase(gid);
    }
}

int Project::tile_flags(const std::string& ts_id, int idx) const {
    if (idx < 0) return 0;
    auto ts_it = tilesets.find(ts_id);
    if (ts_it == tilesets.end()) return 0;
    const auto& ts = ts_it->second;
    if (!ts.contains("tiles")) return 0;
    const auto& tiles = ts["tiles"].as_array();
    if (idx >= static_cast<int>(tiles.size())) return 0;
    const auto& tile = tiles[idx];
    if (tile.contains("flags")) return tile["flags"].as_int();
    return 0;
}

std::vector<std::vector<int>> Project::collision(const Json& m) const {
    const int w = std::max(0, m["w"].as_int());
    const int h = std::max(0, m["h"].as_int());
    const std::string ts_id = m["tileset"].as_string();
    const Json* ground = m.contains("layers") && m["layers"].is_object() &&
        m["layers"].contains("ground") ? &m["layers"]["ground"] : nullptr;
    const Json* objects = m.contains("layers") && m["layers"].is_object() &&
        m["layers"].contains("objects") ? &m["layers"]["objects"] : nullptr;
    const auto tile_at = [](const Json* layer, int x, int y) {
        if (!layer || !layer->is_array() ||
            y < 0 || y >= static_cast<int>(layer->as_array().size())) return -1;
        const Json& row = (*layer)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 ||
            x >= static_cast<int>(row.as_array().size())) return -1;
        return row[static_cast<size_t>(x)].as_int();
    };
    std::vector<std::vector<int>> out(h, std::vector<int>(w, 0));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int g_idx = tile_at(ground, x, y);
            const int o_idx = tile_at(objects, x, y);
            int f = tile_flags(ts_id, g_idx) | tile_flags(ts_id, o_idx);
            if (f & BRIDGE) {
                out[y][x] = f & ~(WATER | SOLID);
            } else {
                out[y][x] = f;
            }
        }
    }
    return out;
}

std::vector<std::string> Project::validate() const {
    std::vector<std::string> errs;
    return errs;
}

Json new_map(const std::string& mid, int w, int h, const std::string& tileset, const std::string& kind, const std::string& mode, const std::string& style, int fill) {
    Json m;
    m["id"] = mid;
    m["name"] = mid;
    m["kind"] = kind;
    m["mode"] = mode;
    m["style"] = style;
    m["tileset"] = tileset;
    m["w"] = w;
    m["h"] = h;
    m["outdoor"] = (kind == "world" || kind == "town");
    m["climate"] = "temperate";
    m["music"] = nullptr;

    JsonObject layers;
    JsonArray ground_rows;
    for (int y = 0; y < h; ++y) {
        JsonArray row;
        for (int x = 0; x < w; ++x) row.push_back(fill);
        ground_rows.push_back(row);
    }
    layers["ground"] = ground_rows;

    JsonArray obj_rows;
    for (int y = 0; y < h; ++y) {
        JsonArray row;
        for (int x = 0; x < w; ++x) row.push_back(-1);
        obj_rows.push_back(row);
    }
    layers["objects"] = obj_rows;
    layers["above"] = obj_rows;
    m["layers"] = layers;

    JsonArray entities;
    JsonObject spawn;
    spawn["type"] = "spawn";
    spawn["id"] = "start";
    spawn["x"] = w / 2;
    spawn["y"] = h / 2;
    spawn["dir"] = "down";
    entities.push_back(spawn);
    m["entities"] = entities;

    return m;
}

} // namespace aur
