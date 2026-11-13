#include "mapforge.hpp"

#include "project.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace aur
{

    namespace
    {
        // A flat tile of one palette index.
        Json flatTile(const std::string &name, int index, int size)
        {
            Json tile = Json(JsonObject{});
            tile["name"] = name;
            tile["pal"] = 0;
            const std::string row(static_cast<size_t>(size), static_cast<char>('0' + index));
            JsonArray rows;
            for (int y = 0; y < size; ++y)
                rows.push_back(Json(row));
            tile["rows"] = rows;
            return tile;
        }

        JsonArray filledRow(int width, int index)
        {
            JsonArray row;
            for (int x = 0; x < width; ++x)
                row.push_back(Json(index));
            return row;
        }

        JsonArray emptyRow(int width)
        {
            JsonArray row;
            for (int x = 0; x < width; ++x)
                row.push_back(Json(-1));
            return row;
        }
    } // namespace

    std::string mapKindName(MapKind kind)
    {
        switch (kind)
        {
        case MapKind::Cave:
            return "cave";
        case MapKind::Platformer:
            return "platformer";
        case MapKind::Town:
        default:
            return "town";
        }
    }

    bool mapKindFromName(const std::string &name, MapKind &kind)
    {
        if (name == "cave") { kind = MapKind::Cave; return true; }
        if (name == "platformer") { kind = MapKind::Platformer; return true; }
        if (name == "town") { kind = MapKind::Town; return true; }
        return false;
    }

    GeneratedMap generate_map_asset(MapKind kind, int seed, int width, int height,
                                    int tile_size, int first_tile_index,
                                    const std::string &map_id, const std::string &name)
    {
        if (width < 4 || width > 256 || height < 4 || height > 256)
            throw std::runtime_error("Map dimensions must be 4-256 tiles.");
        if (tile_size < 8 || tile_size > 64)
            throw std::runtime_error("Tile size must be 8-64 pixels.");

        std::mt19937 rng(static_cast<uint32_t>(seed));
        GeneratedMap generated;

        // The tiles the map needs, in the order they are appended.
        const int ground_tile = first_tile_index;
        const int wall_tile = first_tile_index + 1;
        const int floor_tile = first_tile_index + 2;
        const int platform_tile = first_tile_index + 3;

        Json ground = flatTile(name + "_ground", 2, tile_size);
        Json wall = flatTile(name + "_wall", 3, tile_size);
        wall["flags"] = SOLID;
        Json floor = flatTile(name + "_floor", 3, tile_size);
        floor["flags"] = SOLID;
        Json platform = flatTile(name + "_platform", 4, tile_size);
        platform["flags"] = ONE_WAY;

        JsonArray ground_rows;
        JsonArray object_rows;
        const bool platformer = kind == MapKind::Platformer;
        for (int y = 0; y < height; ++y)
        {
            JsonArray ground_row;
            JsonArray object_row;
            for (int x = 0; x < width; ++x)
            {
                if (platformer)
                {
                    // The floor is the bottom two rows; the rest is air.
                    ground_row.push_back(Json(y >= height - 2 ? floor_tile : -1));
                    object_row.push_back(Json(-1));
                    continue;
                }
                const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
                ground_row.push_back(Json(edge ? wall_tile : ground_tile));
                object_row.push_back(Json(-1));
            }
            ground_rows.push_back(ground_row);
            object_rows.push_back(object_row);
        }

        if (platformer)
        {
            // Two platforms to jump onto, at seeded heights.
            for (int platform = 0; platform < 2; ++platform)
            {
                const int start = 3 + static_cast<int>(rng() % static_cast<uint32_t>(
                                              std::max(1, width - 8)));
                const int length = 3 + static_cast<int>(rng() % 3);
                const int y = 2 + static_cast<int>(rng() % static_cast<uint32_t>(
                                          std::max(1, height / 2)));
                for (int x = start; x < std::min(start + length, width - 1); ++x)
                {
                    Json &row = object_rows[static_cast<size_t>(y)].as_array()[static_cast<size_t>(x)];
                    row = Json(platform_tile);
                }
            }
        }
        else if (kind == MapKind::Town)
        {
            // A few houses, as objects on the ground.
            for (int house = 0; house < 3; ++house)
            {
                const int x = 2 + static_cast<int>(rng() % static_cast<uint32_t>(
                                             std::max(1, width - 5)));
                const int y = 2 + static_cast<int>(rng() % static_cast<uint32_t>(
                                             std::max(1, height - 5)));
                Json &row = object_rows[static_cast<size_t>(y)].as_array()[static_cast<size_t>(x)];
                row = Json(wall_tile);
            }
        }

        Json layers = Json(JsonObject{});
        layers["ground"] = ground_rows;
        layers["objects"] = object_rows;
        layers["above"] = emptyRow(width);

        Json map = Json(JsonObject{});
        map["id"] = map_id;
        map["name"] = name;
        map["kind"] = kind == MapKind::Cave ? "cave" : "town";
        map["mode"] = platformer ? "platformer" : "action";
        map["style"] = "snes";
        map["w"] = width;
        map["h"] = height;
        map["outdoor"] = kind == MapKind::Town;
        map["climate"] = kind == MapKind::Cave ? "temperate" : "temperate";
        map["music"] = nullptr;
        map["tileset"] = "terra";
        map["layers"] = layers;

        // The spawn, and the exit that leaves.
        JsonArray entities;
        Json spawn = Json(JsonObject{});
        spawn["type"] = "spawn";
        spawn["id"] = "start";
        spawn["x"] = platformer ? 1 : width / 2;
        spawn["y"] = platformer ? height - 3 : height / 2;
        spawn["dir"] = "down";
        entities.push_back(spawn);
        // No exit: an exit without a target is a dead end, and the doors
        // are the owner's to place.
        map["entities"] = entities;

        generated.map = map;
        generated.tiles.push_back(ground);
        generated.tiles.push_back(wall);
        generated.tiles.push_back(floor);
        generated.tiles.push_back(platform);
        return generated;
    }

} // namespace aur
