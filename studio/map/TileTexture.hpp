#pragma once

#include "aur/gfx.hpp"
#include "aur/project.hpp"
#include <QColor>
#include <QImage>
#include <cctype>

namespace aur {

inline QImage make_tileset_tile(const Project& project, const std::string& tileset_id, int tile_index) {
    const auto tileset_it = project.tilesets.find(tileset_id);
    if (tileset_it == project.tilesets.end()) return {};

    const Json& tileset = tileset_it->second;
    const auto& tiles = tileset["tiles"].as_array();
    if (tile_index < 0 || tile_index >= static_cast<int>(tiles.size())) return {};

    const Json& tile = tiles[static_cast<size_t>(tile_index)];
    if (!tile.contains("rows") || !tile["rows"].is_array()) return {};
    const auto& palettes = tileset["palettes"].as_array();
    const int palette_index = tile.contains("pal") ? tile["pal"].as_int() : 0;
    if (palette_index < 0 || palette_index >= static_cast<int>(palettes.size())) return {};

    const auto& palette = palettes[static_cast<size_t>(palette_index)].as_array();
    const auto& rows = tile["rows"].as_array();
    const int size = tileset.contains("tile") ? tileset["tile"].as_int() : 16;
    if (size <= 0 || size > 64 || rows.size() != static_cast<size_t>(size)) return {};

    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int y = 0; y < size; ++y) {
        const Json& row_value = rows[static_cast<size_t>(y)];
        if (!row_value.is_string() || row_value.as_string().size() != static_cast<size_t>(size)) return {};
        const std::string& row = row_value.as_string();
        for (int x = 0; x < size; ++x) {
            const char pixel = static_cast<char>(std::toupper(static_cast<unsigned char>(row[static_cast<size_t>(x)])));
            int color_index = 0;
            if (pixel >= '0' && pixel <= '9') color_index = pixel - '0';
            else if (pixel >= 'A' && pixel <= 'F') color_index = pixel - 'A' + 10;
            if (color_index <= 0 || color_index >= static_cast<int>(palette.size())) continue;

            const Json& color_json = palette[static_cast<size_t>(color_index)];
            if (!color_json.is_string()) return {};
            const Rgb color = hex_to_rgba(color_json.as_string());
            image.setPixelColor(x, y, QColor(color.r, color.g, color.b, color.a));
        }
    }
    return image;
}

} // namespace aur
