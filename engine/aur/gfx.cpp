#include "gfx.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace aur {

const std::string HEX_CHARS = "0123456789ABCDEF";

Rgb snap555(const Rgb& rgb) {
    return {
        ((rgb.r >> 3) << 3) | (rgb.r >> 5),
        ((rgb.g >> 3) << 3) | (rgb.g >> 5),
        ((rgb.b >> 3) << 3) | (rgb.b >> 5),
        rgb.a
    };
}

Rgb hex_to_rgba(const std::string& h_str) {
    std::string h = h_str;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    int r = 0, g = 0, b = 0, a = 255;
    if (h.size() >= 2) r = std::stoi(h.substr(0, 2), nullptr, 16);
    if (h.size() >= 4) g = std::stoi(h.substr(2, 2), nullptr, 16);
    if (h.size() >= 6) b = std::stoi(h.substr(4, 2), nullptr, 16);
    if (h.size() >= 8) a = std::stoi(h.substr(6, 2), nullptr, 16);
    return {r, g, b, a};
}

std::string rgb_to_hex(const Rgb& rgb) {
    std::ostringstream ss;
    ss << "#" << std::hex << std::setfill('0')
       << std::setw(2) << rgb.r
       << std::setw(2) << rgb.g
       << std::setw(2) << rgb.b;
    if (rgb.a != 255) {
        ss << std::setw(2) << rgb.a;
    }
    return ss.str();
}

int to_bgr555(const Rgb& rgb) {
    return ((rgb.b >> 3) << 10) | ((rgb.g >> 3) << 5) | (rgb.r >> 3);
}

std::vector<std::string> snap_palette(const std::vector<std::string>& palette) {
    std::vector<std::string> out;
    for (const auto& c : palette) {
        Rgb a = hex_to_rgba(c);
        Rgb sn = snap555({a.r, a.g, a.b, a.a});
        out.push_back(rgb_to_hex(sn));
    }
    return out;
}

std::vector<std::vector<int>> rows_to_grid(const std::vector<std::string>& rows) {
    std::vector<std::vector<int>> grid;
    for (const auto& row : rows) {
        std::vector<int> r;
        for (char ch : row) {
            if (ch == '.') {
                r.push_back(0);
            } else {
                char upper = std::toupper(ch);
                size_t pos = HEX_CHARS.find(upper);
                r.push_back(pos != std::string::npos ? static_cast<int>(pos) : 0);
            }
        }
        grid.push_back(r);
    }
    return grid;
}

std::vector<std::string> grid_to_rows(const std::vector<std::vector<int>>& grid) {
    std::vector<std::string> rows;
    for (const auto& row : grid) {
        std::string s = "";
        for (int v : row) {
            if (v == 0) s.push_back('.');
            else s.push_back(HEX_CHARS[v % 16]);
        }
        rows.push_back(s);
    }
    return rows;
}

std::vector<std::vector<int>> blank(int w, int h) {
    return std::vector<std::vector<int>>(h, std::vector<int>(w, 0));
}

std::vector<std::vector<int>> flip_h(const std::vector<std::vector<int>>& grid) {
    std::vector<std::vector<int>> out;
    for (auto row : grid) {
        std::reverse(row.begin(), row.end());
        out.push_back(row);
    }
    return out;
}

Json new_graphic(int w, int h, const std::vector<std::string>& palette, const std::vector<int>& origin) {
    std::vector<std::string> pal = palette;
    while (pal.size() < 16) pal.push_back("#000000");
    if (pal.size() > 16) pal.resize(16);

    Json g;
    g["w"] = w;
    g["h"] = h;
    JsonArray j_pal;
    for (const auto& c : pal) j_pal.push_back(c);
    g["palette"] = j_pal;

    JsonObject frames;
    auto b_grid = blank(w, h);
    JsonArray j_rows;
    for (const auto& r : grid_to_rows(b_grid)) j_rows.push_back(r);
    frames["0"] = j_rows;
    g["frames"] = frames;
    g["anims"] = JsonObject{};

    JsonArray j_orig;
    j_orig.push_back(origin.size() > 0 ? origin[0] : w / 2);
    j_orig.push_back(origin.size() > 1 ? origin[1] : h);
    g["origin"] = j_orig;

    return g;
}

std::vector<std::string> validate_graphic(const Json& g) {
    std::vector<std::string> errs;
    if (g["palette"].as_array().size() != 16) {
        errs.push_back("palette must have exactly 16 entries");
    }
    for (const auto& [name, rows_j] : g["frames"].as_object()) {
        const auto& rows = rows_j.as_array();
        if (rows.size() != static_cast<size_t>(g["h"].as_int())) {
            errs.push_back("frame height mismatch");
        }
    }
    return errs;
}

std::tuple<int, int, std::vector<unsigned char>> frame_rgba(const Json& g, const std::string& frame, const std::vector<std::string>& palette) {
    int w = g["w"].as_int();
    int h = g["h"].as_int();
    std::vector<std::string> pal_strs;
    if (!palette.empty()) {
        pal_strs = palette;
    } else {
        for (const auto& c : g["palette"].as_array()) pal_strs.push_back(c.as_string());
    }
    std::vector<Rgb> pal;
    for (const auto& c : pal_strs) pal.push_back(hex_to_rgba(c));

    std::vector<std::string> row_strs;
    const auto& frames_obj = g["frames"].as_object();
    auto it = frames_obj.find(frame);
    if (it != frames_obj.end()) {
        for (const auto& r : it->second.as_array()) row_strs.push_back(r.as_string());
    }
    auto grid = rows_to_grid(row_strs);
    std::vector<unsigned char> data;
    for (const auto& row : grid) {
        for (int v : row) {
            if (v == 0 || v >= static_cast<int>(pal.size())) {
                data.insert(data.end(), {0, 0, 0, 0});
            } else {
                Rgb c = pal[v];
                data.insert(data.end(), {static_cast<unsigned char>(c.r), static_cast<unsigned char>(c.g), static_cast<unsigned char>(c.b), static_cast<unsigned char>(c.a)});
            }
        }
    }
    return {w, h, data};
}

std::vector<std::string>& tileset_palettes(Json& ts) {
    static std::vector<std::string> dummy;
    if (!ts.contains("palettes")) {
        if (ts.contains("palette")) {
            JsonArray pals;
            pals.push_back(ts["palette"]);
            ts["palettes"] = pals;
        }
    }
    return dummy;
}

std::vector<std::string> tile_palette(const Json& ts, int idx) {
    std::vector<std::string> def = {"#000000"};
    return def;
}

} // namespace aur
