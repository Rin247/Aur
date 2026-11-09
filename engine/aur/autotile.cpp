#include "autotile.hpp"
#include "world_topology.hpp"

#include <algorithm>
#include <cstddef>

namespace aur {

const int NEIGHBOURS[4][3] = {
    {0, -1, N},
    {1, 0, E},
    {0, 1, S},
    {-1, 0, W}
};

std::map<int, std::string> tile_groups(const Json& ts) {
    std::map<int, std::string> out;
    if (ts.contains("autotiles")) {
        for (const auto& [name, g_json] : ts["autotiles"].as_object()) {
            const auto& tiles = g_json["tiles"].as_array();
            for (const auto& t : tiles) {
                out[t.as_int()] = name;
            }
        }
    }
    return out;
}

bool _joined(const Json& ts, const std::map<int, std::string>& groups, const std::string& group, int tile_idx) {
    auto it = groups.find(tile_idx);
    if (it == groups.end()) return false;
    const auto& autotiles = ts["autotiles"].as_object();
    auto g_it = autotiles.find(group);
    if (g_it == autotiles.end()) return it->second == group;
    if (g_it->second.contains("joins")) {
        for (const auto& j : g_it->second["joins"].as_array()) {
            if (j.as_string() == it->second) return true;
        }
    }
    return it->second == group;
}

int mask_at(const Json& m, const Json& ts, const std::string& layer, int x, int y, const std::string& group, const std::map<int, std::string>& groups) {
    if (!ts.contains("autotiles") || !ts["autotiles"].is_object()) return 0;
    std::map<int, std::string> grp = groups.empty() ? tile_groups(ts) : groups;
    bool edge_joined = true;
    auto autotiles = ts["autotiles"].as_object();
    auto g_it = autotiles.find(group);
    if (g_it != autotiles.end() && g_it->second.contains("edge_joined")) {
        edge_joined = g_it->second["edge_joined"].as_bool();
    }
    if (group.empty() || !m.contains("layers") || !m["layers"].is_object() ||
        !m["layers"].contains(layer) || !m["layers"][layer].is_array()) return 0;
    const auto& rows = m["layers"][layer].as_array();
    int mw = m["w"].as_int();
    int mh = m["h"].as_int();
    const bool wraps_x = wraps_world_x(m);
    if (mw <= 0 || mh <= 0 || y < 0 || y >= mh ||
        static_cast<int>(rows.size()) < mh ||
        !rows[static_cast<std::size_t>(y)].is_array()) return 0;
    if (wraps_x) x = wrapX(x, mw);
    if (x < 0 || x >= mw) return 0;
    const auto& row = rows[static_cast<std::size_t>(y)].as_array();
    if (static_cast<int>(row.size()) < mw) return 0;
    int mask = 0;
    for (int i = 0; i < 4; ++i) {
        int dx = NEIGHBOURS[i][0];
        int dy = NEIGHBOURS[i][1];
        int bit = NEIGHBOURS[i][2];
        int nx = x + dx;
        int ny = y + dy;
        if (wraps_x && mw > 0) nx = wrapX(nx, mw);
        if (!(0 <= nx && nx < mw && 0 <= ny && ny < mh)) {
            if (edge_joined) mask |= bit;
        } else {
            if (!rows[static_cast<std::size_t>(ny)].is_array()) continue;
            const auto& neighbor_row = rows[static_cast<std::size_t>(ny)].as_array();
            if (static_cast<std::size_t>(nx) >= neighbor_row.size()) continue;
            int neighbor_tile = neighbor_row[static_cast<std::size_t>(nx)].as_int();
            if (_joined(ts, grp, group, neighbor_tile)) {
                mask |= bit;
            }
        }
    }
    return mask;
}

void retile(Json& m, const Json& ts, const std::string& layer, int x, int y, const std::map<int, std::string>& groups) {
    if (!m.contains("layers") || !m["layers"].is_object() ||
        !m["layers"].contains(layer) || !m["layers"][layer].is_array()) return;
    int mw = m["w"].as_int();
    int mh = m["h"].as_int();
    if (wraps_world_x(m) && mw > 0) x = wrapX(x, mw);
    if (!(0 <= x && x < mw && 0 <= y && y < mh) ||
        y >= static_cast<int>(m["layers"][layer].as_array().size())) return;
    auto& current_row = m["layers"][layer][static_cast<std::size_t>(y)];
    if (!current_row.is_array() || x >= static_cast<int>(current_row.as_array().size())) return;
    std::map<int, std::string> grp = groups.empty() ? tile_groups(ts) : groups;
    int current_tile = current_row[static_cast<std::size_t>(x)].as_int();
    auto it = grp.find(current_tile);
    if (it != grp.end()) {
        std::string group = it->second;
        int mask = mask_at(m, ts, layer, x, y, group, grp);
        const auto& autotiles = ts["autotiles"].as_object();
        auto g_it = autotiles.find(group);
        if (g_it != autotiles.end()) {
            const auto& tile_indices = g_it->second["tiles"].as_array();
            if (mask >= 0 && static_cast<size_t>(mask) < tile_indices.size()) {
                m["layers"][layer][y][x] = tile_indices[mask].as_int();
            }
        }
    }
}

void paint(Json& m, const Json& ts, const std::string& layer, int x, int y, const std::string& group) {
    if (!m.contains("layers") || !m["layers"].is_object() ||
        !m["layers"].contains(layer) || !m["layers"][layer].is_array() ||
        !ts.contains("autotiles") || !ts["autotiles"].is_object() ||
        !ts["autotiles"].contains(group) || !ts["autotiles"][group].contains("tiles") ||
        !ts["autotiles"][group]["tiles"].is_array() ||
        ts["autotiles"][group]["tiles"].as_array().empty()) return;
    int mw = m["w"].as_int();
    int mh = m["h"].as_int();
    if (wraps_world_x(m) && mw > 0) x = wrapX(x, mw);
    if (!(0 <= x && x < mw && 0 <= y && y < mh)) return;
    std::map<int, std::string> grp = tile_groups(ts);
    if (!ts.contains("autotiles") || !ts["autotiles"].is_object()) return;
    const auto& autotiles = ts["autotiles"].as_object();
    auto g_it = autotiles.find(group);
    if (g_it != autotiles.end()) {
        const Json& row = m["layers"][layer][static_cast<std::size_t>(y)];
        if (!row.is_array() || x >= static_cast<int>(row.as_array().size())) return;
        int first_tile = g_it->second["tiles"].as_array()[0].as_int();
        m["layers"][layer][y][x] = first_tile;
        retile(m, ts, layer, x, y, grp);
        for (int i = 0; i < 4; ++i) {
            retile(m, ts, layer, x + NEIGHBOURS[i][0], y + NEIGHBOURS[i][1], grp);
        }
    }
}

void erase(Json& m, const Json& ts, const std::string& layer, int x, int y, int fill) {
    if (!m.contains("layers") || !m["layers"].is_object() ||
        !m["layers"].contains(layer) || !m["layers"][layer].is_array()) return;
    int mw = m["w"].as_int();
    int mh = m["h"].as_int();
    if (wraps_world_x(m) && mw > 0) x = wrapX(x, mw);
    if (!(0 <= x && x < mw && 0 <= y && y < mh)) return;
    std::map<int, std::string> grp = tile_groups(ts);
    if (y >= static_cast<int>(m["layers"][layer].as_array().size()) ||
        !m["layers"][layer][static_cast<std::size_t>(y)].is_array() ||
        x >= static_cast<int>(m["layers"][layer][static_cast<std::size_t>(y)].as_array().size())) return;
    m["layers"][layer][y][x] = fill;
    for (int i = 0; i < 4; ++i) {
        retile(m, ts, layer, x + NEIGHBOURS[i][0], y + NEIGHBOURS[i][1], grp);
    }
}

void retile_all(Json& m, const Json& ts, const std::string& layer) {
    if (!m.contains("layers") || !m["layers"].is_object() ||
        !m["layers"].contains(layer) || !m["layers"][layer].is_array()) return;
    std::map<int, std::string> grp = tile_groups(ts);
    int mw = m["w"].as_int();
    int mh = m["h"].as_int();
    for (int y = 0; y < mh; ++y) {
        for (int x = 0; x < mw; ++x) {
            retile(m, ts, layer, x, y, grp);
        }
    }
}

} // namespace aur
