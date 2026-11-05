#pragma once

#include "json.hpp"
#include <string>
#include <vector>
#include <map>
#include <tuple>

namespace aur {

struct Rgb {
    int r = 0, g = 0, b = 0, a = 255;
};

Rgb snap555(const Rgb& rgb);
Rgb hex_to_rgba(const std::string& h);
std::string rgb_to_hex(const Rgb& rgb);
int to_bgr555(const Rgb& rgb);
std::vector<std::string> snap_palette(const std::vector<std::string>& palette);

std::vector<std::vector<int>> rows_to_grid(const std::vector<std::string>& rows);
std::vector<std::string> grid_to_rows(const std::vector<std::vector<int>>& grid);
std::vector<std::vector<int>> blank(int w, int h);
std::vector<std::vector<int>> flip_h(const std::vector<std::vector<int>>& grid);

Json new_graphic(int w = 16, int h = 16, const std::vector<std::string>& palette = {}, const std::vector<int>& origin = {});
std::vector<std::string> validate_graphic(const Json& g);
std::tuple<int, int, std::vector<unsigned char>> frame_rgba(const Json& g, const std::string& frame, const std::vector<std::string>& palette = {});

std::vector<std::string>& tileset_palettes(Json& ts);
std::vector<std::string> tile_palette(const Json& ts, int idx);

} // namespace aur
