#include "mcp.hpp"
#include "world_topology.hpp"
#include "forge.hpp"
#include "mapforge.hpp"

#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace aur {
namespace {

constexpr const char* protocol_version = "2024-11-05";

Json make_object() {
    return Json(JsonObject{});
}

Json string_property(const std::string& description) {
    Json property = make_object();
    property["type"] = "string";
    property["description"] = description;
    return property;
}

Json integer_property(const std::string& description, int minimum = 0,
                      int maximum = 0) {
    Json property = make_object();
    property["type"] = "integer";
    property["description"] = description;
    property["minimum"] = minimum;
    if (maximum > minimum) property["maximum"] = maximum;
    return property;
}

Json object_schema(const JsonObject& properties, const JsonArray& required = {}) {
    Json schema = make_object();
    schema["type"] = "object";
    schema["properties"] = properties;
    schema["required"] = required;
    schema["additionalProperties"] = false;
    return schema;
}

Json tool_definition(const std::string& name, const std::string& description,
                    const JsonObject& properties, const JsonArray& required = {}) {
    Json tool = make_object();
    tool["name"] = name;
    tool["description"] = description;
    tool["inputSchema"] = object_schema(properties, required);
    return tool;
}

Json tools_list() {
    JsonArray tools;
    auto add = [&tools](const std::string& name, const std::string& description,
                        const JsonObject& properties, const JsonArray& required = {}) {
        tools.push_back(tool_definition(name, description, properties, required));
    };
    auto required = [](std::initializer_list<const char*> names) {
        JsonArray values;
        for (const char* name : names) values.emplace_back(name);
        return values;
    };

    add("project_overview", "Get project title and counts of maps, tilesets, graphics, and characters.", {}, {});
    add("maps_list", "List project maps with dimensions, kind, and tileset.", {}, {});
    add("map_get", "Read a complete map, including layers and location/exit entities.",
        {{"map_id", string_property("Stable map identifier.")}}, required({"map_id"}));
    add("map_generate", "Generate a seeded map (town, cave or platformer room) and append the tiles it needs.",
        {{"map_id", string_property("Stable map identifier.")},
         {"name", string_property("Display name (defaults to the kind and the seed).")},
         {"kind", string_property("town, cave, or platformer.")},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)},
         {"width", integer_property("Width in tiles, 8-128 (default 40).", 8, 128)},
         {"height", integer_property("Height in tiles, 8-96 (default 24).", 8, 96)},
         {"tileset_id", string_property("Tileset to append the map's tiles to (default terra).")}},
        required({"map_id", "kind", "seed"}));

    add("biomes_list", "List project biome definitions with climate, tileset, base tile, and editor color.", {}, {});
    add("biome_put", "Create or replace a biome definition. The tileset and base tile must exist in the project.",
        {{"biome_id", string_property("Stable biome identifier.")},
         {"biome", Json(JsonObject{{"type", "object"},
                                   {"description", "Biome object with name, climate, tileset, base_tile, color, and optional description."}})}},
        required({"biome_id", "biome"}));

    Json biome_edit_schema = make_object();
    biome_edit_schema["type"] = "object";
    biome_edit_schema["properties"] = JsonObject{
        {"x", integer_property("Tile column.")},
        {"y", integer_property("Tile row.")},
        {"biome_id", string_property("Biome identifier to paint.")}
    };
    biome_edit_schema["required"] = required({"x", "y", "biome_id"});
    biome_edit_schema["additionalProperties"] = false;
    Json biome_edits_property = make_object();
    biome_edits_property["type"] = "array";
    biome_edits_property["items"] = biome_edit_schema;
    biome_edits_property["maxItems"] = 4096;
    add("biome_apply", "Paint biome regions on a map and save the map.",
        {{"map_id", string_property("Stable map identifier.")},
         {"edits", biome_edits_property}}, required({"map_id", "edits"}));
    add("world_generate", "Create a seeded, spherical world with editable terrain, a high-detail shaded surface, and configurable landform topology.",
        {{"map_id", string_property("New stable map identifier.")},
         {"name", string_property("Display name.")},
         {"width", integer_property(
              "World width in tiles; total world size is limited to 3,276,800 cells.", 24, 2560)},
         {"height", integer_property(
              "World height in tiles; total world size is limited to 3,276,800 cells.", 16, 1280)},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)},
         {"topology", string_property("Landform style: continents, archipelago, supercontinent, or fractured.")},
         {"water_level", integer_property("Sea level from 0 to 100.")},
         {"vegetation", integer_property("Vegetation density from 0 to 100.")},
         {"decay", integer_property("Ruins and blight coverage from 0 to 100.")},
         {"axial_tilt", integer_property("Map equator/pole axis rotation from 0 to 359 degrees.")}},
        required({"map_id", "name", "width", "height", "seed", "water_level", "vegetation", "decay"}));

    Json edit_schema = make_object();
    edit_schema["type"] = "object";
    edit_schema["properties"] = JsonObject{
        {"x", integer_property("Tile column.")},
        {"y", integer_property("Tile row.")},
        {"tile", integer_property("Tile index; -1 clears object/above layers.", -1)}
    };
    edit_schema["required"] = required({"x", "y", "tile"});
    edit_schema["additionalProperties"] = false;
    Json edits_property = make_object();
    edits_property["type"] = "array";
    edits_property["items"] = edit_schema;
    edits_property["maxItems"] = 4096;
    add("map_paint", "Paint a batch of cells on a map layer. Changes are saved to the project.",
        {{"map_id", string_property("Stable map identifier.")},
         {"layer", string_property("Map layer: ground, objects, above, or terrain_overrides on HD worlds.")},
         {"edits", edits_property}}, required({"map_id", "layer", "edits"}));

    add("tilesets_list", "List tilesets, tile names, palette colors, and palette indexes.",
        {{"tileset_id", string_property("Optional tileset identifier; omit to list all.")}}, {});
    add("tile_set", "Create or replace one indexed tile in a tileset. Tile rows use 0-F palette indexes.",
        {{"tileset_id", string_property("Stable tileset identifier.")},
         {"tile_index", integer_property("Existing tile index or the next index to append.")},
         {"tile", Json(JsonObject{{"type", "object"},
                                  {"description", "Tile object with name, rows, flags, and pal."}})}},
        required({"tileset_id", "tile_index", "tile"}));
    add("palette_set", "Change one color in a tileset palette. Colors accept #RRGGBB or #RRGGBBAA.",
        {{"tileset_id", string_property("Stable tileset identifier.")},
         {"palette_index", integer_property("Palette index.")},
         {"color_index", integer_property("Palette slot, from 0 to 15.")},
         {"color", string_property("Hex color.")}},
        required({"tileset_id", "palette_index", "color_index", "color"}));

    add("graphics_list", "List indexed graphics and their dimensions.",
        {}, {});
    add("graphic_get", "Read an indexed graphic, including frames, palette, and origin.",
        {{"graphic_id", string_property("Stable graphic identifier.")}}, required({"graphic_id"}));
    add("graphic_put", "Create or replace an indexed graphic. Each frame must contain one hex/dot row per pixel row.",
        {{"graphic_id", string_property("Stable graphic identifier.")},
         {"graphic", Json(JsonObject{{"type", "object"},
                                     {"description", "Graphic object with w, h, palette, frames, and origin."}})}},
        required({"graphic_id", "graphic"}));

    add("characters_list", "List character records.", {}, {});
    add("character_generate", "Generate and save a palette-swapped 8-direction RPG character sprite with walk frames.",
        {{"character_id", string_property("Stable character identifier.")},
         {"name", string_property("Display name.")},
         {"age_group", string_property("child, teen, adult, or elder.")},
         {"race", string_property("sylvani, solani, gharuk, kaeli, featherfolk, or beastkin; defaults to kaeli.")},
         {"gender", string_property("female, male, non-binary, or unspecified.")},
         {"presentation", string_property("androgynous, feminine, or masculine sprite presentation.")},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)}},
        required({"character_id", "name", "age_group", "seed"}));
    add("character_put", "Create or replace a character record.",
        {{"character_id", string_property("Stable character identifier.")},
         {"character", Json(JsonObject{{"type", "object"}})}},
        required({"character_id", "character"}));
    add("creatures_list", "List creature/enemy records from the project database.", {}, {});
    add("creature_generate", "Generate and save a creature sprite. Supported species include cat and dog pets.",
        {{"creature_id", string_property("Stable creature identifier.")},
         {"name", string_property("Display name.")},
         {"species", string_property("cat, dog, slime, wolf, bat, or golem.")},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)}},
        required({"creature_id", "name", "species", "seed"}));
    add("enemy_generate", "Draw a humanoid enemy sprite (bandit, bandit_archer or bandit_boss) and point its record at it.",
        {{"enemy_id", string_property("Enemy record identifier; the graphic takes the same id.")},
         {"kind", string_property("bandit, bandit_archer, or bandit_boss.")},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)}},
        required({"enemy_id", "kind"}));
    add("effect_generate", "Generate an animated indexed effect graphic (spark, fire, water, slash, impact, heal, magic, dust, or explosion).",
        {{"graphic_id", string_property("Stable graphic identifier.")},
         {"kind", string_property("Effect kind.")},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)}},
        required({"graphic_id", "kind", "seed"}));
    add("tile_generate", "Generate a seeded indexed terrain tile and add or replace it in a tileset.",
        {{"tileset_id", string_property("Stable tileset identifier.")},
         {"tile_index", integer_property("Existing tile index or next index to append.")},
         {"kind", string_property("grass, sand, dirt, stone, brick, planks, water, snow, lava, crystal, foliage, or noise.")},
         {"seed", integer_property("Deterministic generation seed.", -2147483647)}},
        required({"tileset_id", "tile_index", "kind", "seed"}));
    add("creature_put", "Create or replace a creature/enemy database record.",
        {{"creature_id", string_property("Stable creature identifier.")},
         {"creature", Json(JsonObject{{"type", "object"}})}},
        required({"creature_id", "creature"}));

    Json result = make_object();
    result["tools"] = tools;
    return result;
}

std::string quote_json(const std::string& value) {
    std::ostringstream stream;
    stream << '"';
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char ch : value) {
        switch (ch) {
            case '"': stream << "\\\""; break;
            case '\\': stream << "\\\\"; break;
            case '\b': stream << "\\b"; break;
            case '\f': stream << "\\f"; break;
            case '\n': stream << "\\n"; break;
            case '\r': stream << "\\r"; break;
            case '\t': stream << "\\t"; break;
            default:
                if (ch < 0x20) {
                    stream << "\\u00" << hex[ch >> 4] << hex[ch & 0x0f];
                } else {
                    stream << static_cast<char>(ch);
                }
        }
    }
    stream << '"';
    return stream.str();
}

std::string compact_json(const Json& value) {
    switch (value.type) {
        case JsonType::Null:
        case JsonType::Bool:
        case JsonType::Number:
            return dump_json(value);
        case JsonType::String:
            return quote_json(value.as_string());
        case JsonType::Array: {
            std::string output = "[";
            const auto& array = value.as_array();
            for (size_t index = 0; index < array.size(); ++index) {
                if (index > 0) output += ',';
                output += compact_json(array[index]);
            }
            output += ']';
            return output;
        }
        case JsonType::Object: {
            std::string output = "{";
            bool first = true;
            for (const auto& [key, item] : value.as_object()) {
                if (!first) output += ',';
                first = false;
                output += quote_json(key) + ':' + compact_json(item);
            }
            output += '}';
            return output;
        }
    }
    return "null";
}

bool valid_id(const std::string& value) {
    if (value.empty() || value.size() > 80) return false;
    for (unsigned char ch : value) {
        if (!std::isalnum(ch) && ch != '_' && ch != '-') return false;
    }
    return true;
}

const Json& require_value(const Json& object, const std::string& key) {
    if (!object.is_object() || !object.contains(key)) {
        throw std::runtime_error("Missing required argument: " + key);
    }
    return object[key];
}

std::string require_string(const Json& object, const std::string& key) {
    const Json& value = require_value(object, key);
    if (!value.is_string()) throw std::runtime_error("Argument must be a string: " + key);
    return value.as_string();
}

int require_integer(const Json& object, const std::string& key) {
    const Json& value = require_value(object, key);
    if (!value.is_number() || std::floor(value.as_number()) != value.as_number()) {
        throw std::runtime_error("Argument must be an integer: " + key);
    }
    if (value.as_number() < std::numeric_limits<int>::min() ||
        value.as_number() > std::numeric_limits<int>::max()) {
        throw std::runtime_error("Integer argument is outside supported range: " + key);
    }
    return value.as_int();
}

Json tool_text(const std::string& text, bool is_error = false) {
    Json content = make_object();
    content["type"] = "text";
    content["text"] = text;
    JsonArray contents{content};
    Json result = make_object();
    result["content"] = contents;
    result["isError"] = is_error;
    return result;
}

Json tool_json(const Json& value) {
    return tool_text(compact_json(value));
}

bool valid_color(const std::string& color) {
    if ((color.size() != 7 && color.size() != 9) || color[0] != '#') return false;
    for (size_t index = 1; index < color.size(); ++index) {
        if (!std::isxdigit(static_cast<unsigned char>(color[index]))) return false;
    }
    return true;
}

int tileset_tile_size(const Json& tileset) {
    const int size = tileset.contains("tile") ? tileset["tile"].as_int() : 16;
    if (size <= 0 || size > 64) throw std::runtime_error("Tileset has an invalid tile size.");
    return size;
}

void validate_tile(const Json& tile, int tile_size, size_t palette_count) {
    if (!tile.is_object() || !tile.contains("rows") || !tile["rows"].is_array()) {
        throw std::runtime_error("Tile must contain a rows array.");
    }
    if (!tile.contains("name") || !tile["name"].is_string() ||
        !tile.contains("flags") || !tile["flags"].is_number() ||
        !tile.contains("pal") || !tile["pal"].is_number()) {
        throw std::runtime_error("Tile must contain name, flags, and pal fields.");
    }
    const int palette = tile["pal"].as_int();
    if (palette < 0 || static_cast<size_t>(palette) >= palette_count) {
        throw std::runtime_error("Tile palette index is out of range.");
    }
    const auto& rows = tile["rows"].as_array();
    if (rows.size() != static_cast<size_t>(tile_size)) {
        throw std::runtime_error("Tile row count must match the tileset tile size.");
    }
    for (const Json& row : rows) {
        if (!row.is_string() || row.as_string().size() != static_cast<size_t>(tile_size)) {
            throw std::runtime_error("Each tile row must match the tileset tile size.");
        }
        for (unsigned char pixel : row.as_string()) {
            if (!std::isxdigit(pixel)) throw std::runtime_error("Tile rows may only contain palette indexes 0-F.");
        }
    }
}

void validate_graphic_payload(const Json& graphic) {
    if (!graphic.is_object() || !graphic.contains("w") || !graphic["w"].is_number() ||
        !graphic.contains("h") || !graphic["h"].is_number()) {
        throw std::runtime_error("Graphic must contain numeric w and h fields.");
    }
    const int width = graphic["w"].as_int();
    const int height = graphic["h"].as_int();
    if (width < 1 || height < 1 || width > 4096 || height > 4096) {
        throw std::runtime_error("Graphic dimensions must be between 1 and 4096 pixels.");
    }
    if (!graphic.contains("palette") || !graphic["palette"].is_array() ||
        graphic["palette"].as_array().size() != 16) {
        throw std::runtime_error("Graphic palette must contain exactly 16 colors.");
    }
    for (const Json& color : graphic["palette"].as_array()) {
        if (!color.is_string() || !valid_color(color.as_string())) {
            throw std::runtime_error("Graphic palette colors must use #RRGGBB or #RRGGBBAA.");
        }
    }
    if (!graphic.contains("frames") || !graphic["frames"].is_object() ||
        graphic["frames"].as_object().empty()) {
        throw std::runtime_error("Graphic must contain at least one frame.");
    }
    if (graphic["frames"].as_object().size() > 256 ||
        static_cast<size_t>(width) * static_cast<size_t>(height) *
                graphic["frames"].as_object().size() > 16U * 1024U * 1024U) {
        throw std::runtime_error("Graphic frame data exceeds the 16-megapixel safety limit.");
    }
    for (const auto& [frame_id, frame] : graphic["frames"].as_object()) {
        if (!valid_id(frame_id) || !frame.is_array() ||
            frame.as_array().size() != static_cast<size_t>(height)) {
            throw std::runtime_error("Each graphic frame must have h rows and a valid frame identifier.");
        }
        for (const Json& row : frame.as_array()) {
            if (!row.is_string() || row.as_string().size() != static_cast<size_t>(width)) {
                throw std::runtime_error("Graphic frame rows must have exactly w pixels.");
            }
            for (unsigned char pixel : row.as_string()) {
                if (pixel != '.' && !std::isxdigit(pixel)) {
                    throw std::runtime_error("Graphic pixels may only contain . or palette indexes 0-F.");
                }
            }
        }
    }
}

Json records_list(const JsonObject& records) {
    JsonArray entries;
    for (const auto& [id, record] : records) {
        Json entry = make_object();
        entry["id"] = id;
        entry["data"] = record;
        entries.push_back(entry);
    }
    return Json(entries);
}

Json execute_tool(Project& project, const std::string& name, const Json& args, bool& changed) {
    changed = false;
    if (!args.is_object()) throw std::runtime_error("Tool arguments must be a JSON object.");

    if (name == "project_overview") {
        Json result = make_object();
        result["title"] = project.meta.contains("title") ? project.meta["title"] : Json("Untitled");
        result["map_count"] = static_cast<int>(project.maps.size());
        result["tileset_count"] = static_cast<int>(project.tilesets.size());
        result["graphic_count"] = static_cast<int>(project.graphics.size());
        result["character_count"] = static_cast<int>(project.characters.size());
        return result;
    }
    if (name == "maps_list") {
        JsonArray maps;
        for (const auto& [id, map] : project.maps) {
            Json item = make_object();
            item["id"] = id;
            item["name"] = map.contains("name") ? map["name"] : Json(id);
            item["kind"] = map.contains("kind") ? map["kind"] : Json("unknown");
            item["w"] = map["w"];
            item["h"] = map["h"];
            item["tileset"] = map["tileset"];
            maps.push_back(item);
        }
        return Json(maps);
    }
    if (name == "map_get") {
        const std::string id = require_string(args, "map_id");
        const auto it = project.maps.find(id);
        if (it == project.maps.end()) throw std::runtime_error("Map not found: " + id);
        return it->second;
    }
    if (name == "biomes_list") return records_list(project.biomes);
    if (name == "biome_put") {
        const std::string id = require_string(args, "biome_id");
        const Json& biome = require_value(args, "biome");
        if (!valid_id(id) || !biome.is_object() ||
            !biome["name"].is_string() || biome["name"].as_string().empty() ||
            !biome["climate"].is_string() || !biome["tileset"].is_string() ||
            !biome["base_tile"].is_number() ||
            std::floor(biome["base_tile"].as_number()) != biome["base_tile"].as_number() ||
            !biome["color"].is_string() || !valid_color(biome["color"].as_string())) {
            throw std::runtime_error("Biome must have a valid identifier, name, climate, tileset, integer base_tile, and #RRGGBB color.");
        }
        const auto tileset_it = project.tilesets.find(biome["tileset"].as_string());
        if (tileset_it == project.tilesets.end() ||
            !tileset_it->second.contains("tiles") ||
            biome["base_tile"].as_number() < 0 ||
            biome["base_tile"].as_number() >= static_cast<double>(tileset_it->second["tiles"].as_array().size())) {
            throw std::runtime_error("Biome tileset or base tile does not exist.");
        }
        for (const std::string key : {"vegetation", "decay", "water_level"}) {
            if (!biome.contains(key)) continue;
            const Json& value = biome[key];
            if (!value.is_number() || std::floor(value.as_number()) != value.as_number() ||
                value.as_number() < 0 || value.as_number() > 100) {
                throw std::runtime_error("Biome generation settings must be integers from 0 to 100.");
            }
        }
        project.biomes[id] = biome;
        changed = true;
        Json result = make_object();
        result["biome_id"] = id;
        return result;
    }
    if (name == "biome_apply") {
        const std::string map_id = require_string(args, "map_id");
        auto map_it = project.maps.find(map_id);
        if (map_it == project.maps.end()) throw std::runtime_error("Map not found: " + map_id);
        const Json& edits_value = require_value(args, "edits");
        if (!edits_value.is_array() || edits_value.as_array().empty() ||
            edits_value.as_array().size() > 4096) {
            throw std::runtime_error("edits must contain between 1 and 4096 biome edits.");
        }
        const Json& map = map_it->second;
        const int width = map["w"].as_int();
        const int height = map["h"].as_int();
        const bool wraps_x = wraps_world_x(map);
        const std::string tileset_id = map["tileset"].as_string();
        if (width <= 0 || height <= 0 || !map.contains("layers") ||
            !map["layers"].is_object()) {
            throw std::runtime_error("Map layer data is missing or invalid.");
        }
        const bool hd_surface = map.contains("worldgen") &&
            map["worldgen"].is_object() && map["worldgen"].contains("style") &&
            map["worldgen"]["style"].as_string() == "hd2d" &&
            map["layers"].contains("elevation") && map["layers"]["elevation"].is_array();
        if (map["layers"].contains("ground")) {
            const Json& ground_layer = map["layers"]["ground"];
            if (!ground_layer.is_array() ||
                static_cast<int>(ground_layer.as_array().size()) != height) {
                if (!hd_surface) {
                    throw std::runtime_error("Map ground layer data is missing or invalid.");
                }
            } else {
                for (const Json& row : ground_layer.as_array()) {
                    if (!row.is_array() || static_cast<int>(row.as_array().size()) != width) {
                        if (!hd_surface) {
                            throw std::runtime_error(
                                "Map ground layer dimensions do not match map metadata.");
                        }
                        break;
                    }
                }
            }
        } else if (!hd_surface) {
            throw std::runtime_error("Map ground layer data is missing or invalid.");
        }
        if (map["layers"].contains("biomes")) {
            const Json& biome_layer = map["layers"]["biomes"];
            if (!biome_layer.is_array() || static_cast<int>(biome_layer.as_array().size()) != height) {
                throw std::runtime_error("Map biome layer dimensions do not match map metadata.");
            }
            for (const Json& row : biome_layer.as_array()) {
                if (!row.is_array() || static_cast<int>(row.as_array().size()) != width) {
                    throw std::runtime_error("Map biome layer dimensions do not match map metadata.");
                }
            }
        }
        struct BiomeEdit { int x; int y; std::string id; };
        std::vector<BiomeEdit> edits;
        edits.reserve(edits_value.as_array().size());
        for (const Json& edit : edits_value.as_array()) {
            const int x = require_integer(edit, "x");
            const int y = require_integer(edit, "y");
            const std::string biome_id = require_string(edit, "biome_id");
            if ((!wraps_x && (x < 0 || x >= width)) || y < 0 || y >= height) {
                throw std::runtime_error("Biome edit is outside map bounds.");
            }
            const auto biome_it = project.biomes.find(biome_id);
            if (biome_it == project.biomes.end()) throw std::runtime_error("Biome not found: " + biome_id);
            if (biome_it->second["tileset"].as_string() != tileset_id) {
                throw std::runtime_error("Biome tileset does not match map tileset.");
            }
            const auto tileset_it = project.tilesets.find(tileset_id);
            if (tileset_it == project.tilesets.end() ||
                !biome_it->second["base_tile"].is_number() ||
                biome_it->second["base_tile"].as_number() < 0 ||
                biome_it->second["base_tile"].as_number() >=
                    static_cast<double>(tileset_it->second["tiles"].as_array().size())) {
                throw std::runtime_error("Biome base tile is outside the map tileset range.");
            }
            edits.push_back({wraps_x ? wrapX(x, width) : x, y, biome_id});
        }
        for (const BiomeEdit& edit : edits) paint_biome(project, map_id, edit.x, edit.y, edit.id);
        changed = true;
        Json result = make_object();
        result["map_id"] = map_id;
        result["changed_cells"] = static_cast<int>(edits.size());
        return result;
    }
    if (name == "map_generate") {
        const std::string id = require_string(args, "map_id");
        if (!valid_id(id)) throw std::runtime_error("Map identifier must be letters, numbers, _ or -.");
        if (project.maps.contains(id)) throw std::runtime_error("Map already exists: " + id);
        const std::string kind_name = require_string(args, "kind");
        MapKind kind = MapKind::Town;
        if (!mapKindFromName(kind_name, kind)) throw std::runtime_error("Unknown map kind: " + kind_name);
        const int seed = require_integer(args, "seed");
        const int width = args.contains("width") ? require_integer(args, "width") : 40;
        const int height = args.contains("height") ? require_integer(args, "height") : 24;
        const std::string tileset_id = args.contains("tileset_id") ? require_string(args, "tileset_id") : "terra";
        auto tileset_it = project.tilesets.find(tileset_id);
        if (tileset_it == project.tilesets.end()) throw std::runtime_error("Tileset not found: " + tileset_id);
        Json& tileset = tileset_it->second;
        if (!tileset["tiles"].is_array() || !tileset["palettes"].is_array() ||
            tileset["palettes"].as_array().empty())
            throw std::runtime_error("Tileset must have tiles and at least one palette bank.");
        const std::string display_name = args.contains("name") ? require_string(args, "name") : mapKindName(kind) + " " + std::to_string(seed);
        auto& tiles = tileset["tiles"].as_array();
        const int first_tile = static_cast<int>(tiles.size());
        const int tile_size = tileset_tile_size(tileset);
        const TerrainPick pick = (args.contains("coast_side") && args["coast_side"].is_string())
            ? pick_for_coast_side(args["coast_side"].as_string()) : TerrainPick{};
        GeneratedMap generated = generate_map_asset(kind, seed, width, height, tile_size, first_tile,
                                                    id, display_name, pick.group.empty() ? nullptr : &pick);
        for (const Json& tile : generated.tiles) { validate_tile(tile, tile_size, tileset["palettes"].as_array().size()); tiles.push_back(tile); }
        if (!pick.group.empty()) apply_coastal_beach(generated.map, tileset, pick, seed);
        project.maps[id] = generated.map;
        changed = true;
        Json result = make_object();
        result["map_id"] = id;
        result["name"] = display_name;
        result["kind"] = mapKindName(kind);
        result["mode"] = generated.map["mode"];
        result["first_tile_index"] = first_tile;
        result["tiles_added"] = static_cast<int>(generated.tiles.size());
        return result;
    }
    if (name == "world_generate") {
        const std::string id = require_string(args, "map_id");
        const std::string display_name = require_string(args, "name");
        if (!valid_id(id) || display_name.empty()) {
            throw std::runtime_error("World map id and name are required.");
        }
        if (project.maps.contains(id)) throw std::runtime_error("Map already exists: " + id);
        const auto world_tileset = project.tilesets.find("world");
        if (world_tileset == project.tilesets.end() ||
            !world_tileset->second.contains("tiles") ||
            world_tileset->second["tiles"].as_array().size() < 8) {
            throw std::runtime_error("World generation requires a world tileset with at least 8 terrain tiles.");
        }
        const std::string topology = args.contains("topology")
            ? require_string(args, "topology") : "continents";
        Json map = generate_world_map(id, display_name,
            require_integer(args, "width"), require_integer(args, "height"),
            require_integer(args, "seed"), topology, require_integer(args, "water_level"),
            require_integer(args, "vegetation"), require_integer(args, "decay"),
            args.contains("axial_tilt") ? require_integer(args, "axial_tilt") : 0);
        project.maps[id] = map;
        changed = true;
        Json result = make_object();
        result["map_id"] = id;
        result["name"] = display_name;
        result["seed"] = args["seed"];
        result["worldgen"] = map["worldgen"];
        return result;
    }
    if (name == "map_paint") {
        const std::string map_id = require_string(args, "map_id");
        const std::string layer = require_string(args, "layer");
        auto map_it = project.maps.find(map_id);
        if (map_it == project.maps.end()) throw std::runtime_error("Map not found: " + map_id);
        Json& map = map_it->second;
        const bool hd_surface = map.contains("worldgen") &&
            map["worldgen"].is_object() && map["worldgen"].contains("style") &&
            map["worldgen"]["style"].as_string() == "hd2d" &&
            map.contains("layers") && map["layers"].is_object() &&
            map["layers"].contains("elevation") && map["layers"]["elevation"].is_array();
        if (layer != "ground" && layer != "objects" && layer != "above" &&
            !(layer == "terrain_overrides" && hd_surface)) {
            throw std::runtime_error(
                "Layer must be ground, objects, or above; HD worlds also support terrain_overrides.");
        }
        const int width = map["w"].as_int();
        const int height = map["h"].as_int();
        const bool wraps_x = wraps_world_x(map);
        if (width <= 0 || height <= 0 || !map.contains("layers") ||
            !map["layers"].is_object() || !map["layers"].contains(layer)) {
            throw std::runtime_error("Map layer data is missing or invalid.");
        }
        const auto tile_set = project.tilesets.find(map["tileset"].as_string());
        if (tile_set == project.tilesets.end()) throw std::runtime_error("Map tileset not found.");
        const int tile_count = static_cast<int>(tile_set->second["tiles"].as_array().size());

        const Json& edits_value = require_value(args, "edits");
        if (!edits_value.is_array() || edits_value.as_array().empty() ||
            edits_value.as_array().size() > 4096) {
            throw std::runtime_error("edits must contain between 1 and 4096 cell edits.");
        }
        struct Edit { int x; int y; int tile; };
        std::vector<Edit> edits;
        edits.reserve(edits_value.as_array().size());
        for (const Json& edit : edits_value.as_array()) {
            const int x = require_integer(edit, "x");
            const int y = require_integer(edit, "y");
            const int tile = require_integer(edit, "tile");
            if ((!wraps_x && (x < 0 || x >= width)) || y < 0 || y >= height) {
                throw std::runtime_error("Tile edit is outside map bounds.");
            }
            const int world_x = wraps_x ? wrapX(x, width) : x;
            if (((layer == "ground" || layer == "terrain_overrides") &&
                 (tile < (layer == "ground" ? 0 : -1) || tile >= tile_count)) ||
                (layer != "ground" && layer != "terrain_overrides" &&
                 (tile < -1 || tile >= tile_count))) {
                throw std::runtime_error("Tile index is outside the tileset range.");
            }
            const auto& rows = map["layers"][layer].as_array();
            if (y >= static_cast<int>(rows.size()) ||
                !rows[static_cast<size_t>(y)].is_array() ||
                world_x >= static_cast<int>(rows[static_cast<size_t>(y)].as_array().size())) {
                throw std::runtime_error("Map layer dimensions do not match map metadata.");
            }
            edits.push_back({world_x, y, tile});
        }
        for (const Edit& edit : edits) {
            map["layers"][layer][static_cast<size_t>(edit.y)][static_cast<size_t>(edit.x)] = edit.tile;
        }
        changed = true;
        Json result = make_object();
        result["map_id"] = map_id;
        result["layer"] = layer;
        result["changed_cells"] = static_cast<int>(edits.size());
        return result;
    }
    if (name == "tilesets_list") {
        JsonArray tilesets;
        const bool filter = args.contains("tileset_id");
        const std::string filter_id = filter ? require_string(args, "tileset_id") : "";
        for (const auto& [id, tileset] : project.tilesets) {
            if (filter && id != filter_id) continue;
            Json item = make_object();
            item["id"] = id;
            item["name"] = tileset.contains("name") ? tileset["name"] : Json(id);
            item["tile_size"] = tileset.contains("tile") ? tileset["tile"] : Json(16);
            item["tiles"] = tileset["tiles"];
            item["palettes"] = tileset["palettes"];
            tilesets.push_back(item);
        }
        if (filter && tilesets.empty()) throw std::runtime_error("Tileset not found: " + filter_id);
        return Json(tilesets);
    }
    if (name == "tile_set") {
        const std::string tileset_id = require_string(args, "tileset_id");
        const int tile_index = require_integer(args, "tile_index");
        auto tileset_it = project.tilesets.find(tileset_id);
        if (tileset_it == project.tilesets.end()) throw std::runtime_error("Tileset not found: " + tileset_id);
        Json& tileset = tileset_it->second;
        auto& tiles = tileset["tiles"].as_array();
        if (tile_index < 0 || tile_index > static_cast<int>(tiles.size()) || tile_index >= 4096) {
            throw std::runtime_error("tile_index must reference an existing tile or the next tile to append.");
        }
        const auto& palettes = tileset["palettes"].as_array();
        validate_tile(require_value(args, "tile"), tileset_tile_size(tileset), palettes.size());
        if (tile_index == static_cast<int>(tiles.size())) tiles.push_back(args["tile"]);
        else tiles[static_cast<size_t>(tile_index)] = args["tile"];
        changed = true;
        Json result = make_object();
        result["tileset_id"] = tileset_id;
        result["tile_index"] = tile_index;
        return result;
    }
    if (name == "palette_set") {
        const std::string tileset_id = require_string(args, "tileset_id");
        const int palette_index = require_integer(args, "palette_index");
        const int color_index = require_integer(args, "color_index");
        const std::string color = require_string(args, "color");
        auto tileset_it = project.tilesets.find(tileset_id);
        if (tileset_it == project.tilesets.end()) throw std::runtime_error("Tileset not found: " + tileset_id);
        auto& palettes = tileset_it->second["palettes"].as_array();
        if (palette_index < 0 || palette_index >= static_cast<int>(palettes.size())) {
            throw std::runtime_error("Palette index is out of range.");
        }
        auto& palette = palettes[static_cast<size_t>(palette_index)].as_array();
        if (color_index < 0 || color_index >= static_cast<int>(palette.size())) {
            throw std::runtime_error("Color index is out of range.");
        }
        if (!valid_color(color)) throw std::runtime_error("Color must use #RRGGBB or #RRGGBBAA.");
        palette[static_cast<size_t>(color_index)] = color;
        changed = true;
        Json result = make_object();
        result["tileset_id"] = tileset_id;
        result["palette_index"] = palette_index;
        result["color_index"] = color_index;
        result["color"] = color;
        return result;
    }
    if (name == "graphics_list") {
        JsonArray graphics;
        for (const auto& [id, graphic] : project.graphics) {
            Json item = make_object();
            item["id"] = id;
            item["w"] = graphic["w"];
            item["h"] = graphic["h"];
            graphics.push_back(item);
        }
        return Json(graphics);
    }
    if (name == "graphic_get") {
        const std::string id = require_string(args, "graphic_id");
        const auto it = project.graphics.find(id);
        if (it == project.graphics.end()) throw std::runtime_error("Graphic not found: " + id);
        return it->second;
    }
    if (name == "graphic_put") {
        const std::string id = require_string(args, "graphic_id");
        if (!valid_id(id)) throw std::runtime_error("Graphic identifier may only contain letters, numbers, _ and -.");
        const Json& graphic = require_value(args, "graphic");
        validate_graphic_payload(graphic);
        project.graphics[id] = graphic;
        project.invalidate(id);
        changed = true;
        return graphic;
    }
    if (name == "characters_list") return records_list(project.characters);
    if (name == "character_generate") {
        const std::string id = require_string(args, "character_id");
        const std::string display_name = require_string(args, "name");
        const std::string age_group = require_string(args, "age_group");
        const std::string race = args.contains("race") ? require_string(args, "race") : "kaeli";
        const std::string gender = args.contains("gender") ? require_string(args, "gender") : "unspecified";
        const std::string presentation = args.contains("presentation")
            ? require_string(args, "presentation") : "androgynous";
        const int seed = require_integer(args, "seed");
        if (!valid_id(id) || display_name.empty() ||
            (age_group != "child" && age_group != "teen" &&
             age_group != "adult" && age_group != "elder") ||
            !is_supported_character_race(race) ||
            (gender != "female" && gender != "male" &&
             gender != "non-binary" && gender != "unspecified") ||
            (presentation != "androgynous" && presentation != "feminine" &&
             presentation != "masculine")) {
            throw std::runtime_error("Character id, name, age_group, race, gender, or presentation is invalid.");
        }
        Json spec = random_character_spec(seed, display_name, age_group, gender, presentation, race);
        project.graphics[id] = compile_character(spec, project.parts);
        project.characters[id] = Json(JsonObject{
            {"name", display_name}, {"age_group", age_group}, {"race", race}, {"gender", gender},
            {"presentation", presentation}, {"graphic", id},
            {"spec", spec}
        });
        project.invalidate(id);
        changed = true;
        Json result = make_object();
        result["character_id"] = id;
        result["graphic_id"] = id;
        result["age_group"] = age_group;
        result["race"] = race;
        result["gender"] = gender;
        result["presentation"] = presentation;
        return result;
    }
    if (name == "character_put") {
        const std::string id = require_string(args, "character_id");
        const Json& character = require_value(args, "character");
        if (!valid_id(id) || !character.is_object()) {
            throw std::runtime_error("Character identifier must be letters, numbers, _ or -; character must be an object.");
        }
        project.characters[id] = character;
        changed = true;
        Json result = make_object();
        result["character_id"] = id;
        return result;
    }
    if (name == "creatures_list") {
        const Json& enemies = project.db["enemies"];
        return enemies.is_object() ? records_list(enemies.as_object()) : Json(JsonArray{});
    }
    if (name == "creature_generate") {
        const std::string id = require_string(args, "creature_id");
        const std::string display_name = require_string(args, "name");
        const std::string species = require_string(args, "species");
        const int seed = require_integer(args, "seed");
        if (!valid_id(id) || display_name.empty()) {
            throw std::runtime_error("Creature id and name are required.");
        }
        project.graphics[id] = generate_creature_graphic(species, seed);
        project.db["enemies"][id] = Json(JsonObject{
            {"name", display_name}, {"species", species}, {"graphic", id},
            {"pet", species == "cat" || species == "dog"}
        });
        project.invalidate(id);
        changed = true;
        Json result = make_object();
        result["creature_id"] = id;
        result["graphic_id"] = id;
        return result;
    }
    if (name == "enemy_generate") {
        const std::string id = require_string(args, "enemy_id");
        const std::string kind = require_string(args, "kind");
        const int seed = args.contains("seed") ? require_integer(args, "seed") : 1;
        if (!valid_id(id)) throw std::runtime_error("Enemy identifier may only contain letters, numbers, _ and -.");
        project.graphics[id] = generate_enemy_graphic(kind, seed);
        if (project.db["enemies"].is_object() && project.db["enemies"].contains(id)) {
            project.db["enemies"][id]["gfx"] = id;
        }
        project.invalidate(id);
        changed = true;
        Json result = make_object();
        result["enemy_id"] = id;
        result["graphic_id"] = id;
        result["kind"] = kind;
        return result;
    }
    if (name == "effect_generate") {
        const std::string id = require_string(args, "graphic_id");
        const std::string kind = require_string(args, "kind");
        const int seed = require_integer(args, "seed");
        if (!valid_id(id)) throw std::runtime_error("Graphic identifier may only contain letters, numbers, _ and -.");
        project.graphics[id] = generate_effect_graphic(kind, seed);
        project.invalidate(id);
        changed = true;
        Json result = make_object();
        result["graphic_id"] = id;
        result["effect_kind"] = kind;
        return result;
    }
    if (name == "tile_generate") {
        const std::string tileset_id = require_string(args, "tileset_id");
        const int tile_index = require_integer(args, "tile_index");
        const std::string kind = require_string(args, "kind");
        const int seed = require_integer(args, "seed");
        auto tileset_it = project.tilesets.find(tileset_id);
        if (tileset_it == project.tilesets.end()) throw std::runtime_error("Tileset not found: " + tileset_id);
        Json& tileset = tileset_it->second;
        if (!tileset["tiles"].is_array() || !tileset["palettes"].is_array() ||
            tileset["palettes"].as_array().empty()) {
            throw std::runtime_error("Tileset must have tiles and at least one palette bank.");
        }
        auto& tiles = tileset["tiles"].as_array();
        if (tile_index < 0 || tile_index > static_cast<int>(tiles.size()) || tile_index >= 4096) {
            throw std::runtime_error("tile_index must reference an existing tile or the next tile to append.");
        }
        const int tile_size = tileset_tile_size(tileset);
        Json tile = generate_tile_asset(kind, seed, tile_size, tile_size);
        validate_tile(tile, tile_size, tileset["palettes"].as_array().size());
        if (tile_index == static_cast<int>(tiles.size())) tiles.push_back(tile);
        else tiles[static_cast<size_t>(tile_index)] = tile;
        changed = true;
        Json result = make_object();
        result["tileset_id"] = tileset_id;
        result["tile_index"] = tile_index;
        result["tile"] = tile;
        return result;
    }
    if (name == "creature_put") {
        const std::string id = require_string(args, "creature_id");
        const Json& creature = require_value(args, "creature");
        if (!valid_id(id) || !creature.is_object()) {
            throw std::runtime_error("Creature identifier must be letters, numbers, _ or -; creature must be an object.");
        }
        project.db["enemies"][id] = creature;
        changed = true;
        Json result = make_object();
        result["creature_id"] = id;
        return result;
    }

    throw std::runtime_error("Unknown tool: " + name);
}

Json rpc_error(const Json& id, int code, const std::string& message) {
    Json error = make_object();
    error["code"] = code;
    error["message"] = message;
    Json response = make_object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = error;
    return response;
}

Json process_request(Project& project, const Json& request) {
    if (!request.is_object() || !request.contains("method") || !request["method"].is_string()) {
        return rpc_error(request.is_object() ? request["id"] : Json(nullptr), -32600, "Invalid Request");
    }

    const Json id = request.contains("id") ? request["id"] : Json(nullptr);
    const std::string method = request["method"].as_string();
    if (method == "notifications/initialized" || method == "notifications/cancelled") return Json(nullptr);
    if (method == "initialize") {
        Json result = make_object();
        result["protocolVersion"] = protocol_version;
        result["capabilities"] = Json(JsonObject{{"tools", Json(JsonObject{{"listChanged", false}})}});
        result["serverInfo"] = Json(JsonObject{{"name", "aur-project-tools"}, {"version", "1.0.0"}});
        result["instructions"] = "Edits are restricted to the project directory passed at server startup and are saved after each mutation.";
        Json response = make_object();
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = result;
        return response;
    }
    if (method == "ping") {
        Json response = make_object();
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = make_object();
        return response;
    }
    if (method == "tools/list") {
        Json response = make_object();
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = tools_list();
        return response;
    }
    if (method == "tools/call") {
        const Json& params = request["params"];
        try {
            if (!params.is_object()) throw std::runtime_error("Tool call params must be an object.");
            const std::string name = require_string(params, "name");
            const Json empty_args = make_object();
            const Json& args = params.contains("arguments") ? params["arguments"] : empty_args;
            bool changed = false;
            Json tool_result_value = execute_tool(project, name, args, changed);
            if (changed) project.save();
            Json response = make_object();
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["result"] = tool_json(tool_result_value);
            return response;
        } catch (const std::exception& error) {
            Json response = make_object();
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["result"] = tool_text(error.what(), true);
            return response;
        }
    }

    return rpc_error(id, -32601, "Method not found: " + method);
}

} // namespace

void run_mcp_server(Project& project, std::istream& input, std::ostream& output) {
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        Json response;
        try {
            response = process_request(project, parse_json(line));
        } catch (const std::exception& error) {
            response = rpc_error(Json(nullptr), -32700, std::string("Parse error: ") + error.what());
        }
        if (!response.is_null()) {
            output << compact_json(response) << '\n';
            output.flush();
        }
    }
}

} // namespace aur
