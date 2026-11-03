#pragma once

#include "json.hpp"
#include "game.hpp"
#include "autotile.hpp"
#include "project.hpp"
#include "world_topology.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace aur {

class Hero {
public:
    double x = 0, y = 0;
    std::string dir = "down";
    double t = 0.0;
    bool moving = false;
};

class FieldScene : public Scene {
public:
    Game* game;
    Json m;
    std::string ts_id;
    int W = 0, H = 0;
    bool wraps_x = false;
    std::vector<std::vector<int>> flags;
    Hero hero;
    double cam_x = 0, cam_y = 0;
    double t = 0.0;

    FieldScene(Game* game, const Json& m_, const std::string& spawn = "", const std::vector<int>& pos = {}, const std::string& dir = "")
        : game(game), m(m_) {
        ts_id = m["tileset"].as_string();
        W = m["w"].as_int();
        H = m["h"].as_int();
        wraps_x = wraps_world_x(m);
        flags = game->project->collision(m);
        double hx = (W / 2.0 + 0.5) * field_tile_size;
        double hy = (H / 2.0 + 1.0) * field_tile_size - 1.0;
        if (!spawn.empty() && m.contains("entities") && m["entities"].is_array()) {
            for (const Json& entity : m["entities"].as_array()) {
                if (entity["type"].as_string() != "spawn" ||
                    entity["id"].as_string() != spawn) continue;
                hx = (entity["x"].as_int() + 0.5) * field_tile_size;
                hy = (entity["y"].as_int() + 1.0) * field_tile_size - 1.0;
                if (entity.contains("dir")) hero.dir = entity["dir"].as_string();
                break;
            }
        }
        if (!pos.empty() && pos.size() >= 2) {
            hx = pos[0] + field_tile_size / 2.0;
            hy = pos[1] + field_tile_size * 1.5;
        }
        if (!dir.empty()) hero.dir = dir;
        hero.x = hx; hero.y = hy;
        cam_x = hero.x; cam_y = hero.y;
    }

    int tile_flags(double x, double y) const {
        int tx = static_cast<int>(std::floor(x / field_tile_size));
        const int ty = static_cast<int>(std::floor(y / field_tile_size));
        if (ty < 0 || ty >= H || (!wraps_x && (tx < 0 || tx >= W))) return SOLID;
        if (wraps_x) tx = wrapX(tx, W);
        return flags[ty][tx];
    }

    void update(double dt, InputState& inp) override {
        t += dt;
        double ax = 0, ay = 0;
        if (inp.held.count("left")) ax -= 1;
        if (inp.held.count("right")) ax += 1;
        if (inp.held.count("up")) ay -= 1;
        if (inp.held.count("down")) ay += 1;
        double spd = 148.0;
        double len = std::hypot(ax, ay);
        hero.moving = (len > 0);
        if (hero.moving) {
            double nx = ax / len, ny = ay / len;
            if (ax != 0 && ay != 0) {
                hero.dir = (ay > 0 ? "down_" : "up_") + std::string(ax > 0 ? "right" : "left");
            } else if (ax != 0) {
                hero.dir = ax > 0 ? "right" : "left";
            } else {
                hero.dir = ay > 0 ? "down" : "up";
            }
            double nxp = hero.x + nx * spd * dt;
            if (!(tile_flags(nxp, hero.y) & (SOLID | WATER))) hero.x = nxp;
            double nyp = hero.y + ny * spd * dt;
            if (!(tile_flags(hero.x, nyp) & (SOLID | WATER))) hero.y = nyp;
            hero.t += dt;
        } else {
            hero.t = 0.0;
        }
        double vw = game->layout.w, vh = game->layout.h;
        double mw = W * field_tile_size, mh = H * field_tile_size;
        double tx = wraps_x ? hero.x - vw / 2.0
            : (mw > vw) ? std::max(0.0, std::min(mw - vw, hero.x - vw / 2.0))
                        : (mw - vw) / 2.0;
        double ty = (mh > vh) ? std::max(0.0, std::min(mh - vh, hero.y - 24.0 - vh / 2.0)) : (mh - vh) / 2.0;
        cam_x = tx; cam_y = ty;
    }

    void draw(Renderer& r) override {
        r.camera((int)cam_x, (int)cam_y);
        const Json* ground = m.contains("layers") && m["layers"].is_object() &&
            m["layers"].contains("ground") ? &m["layers"]["ground"] : nullptr;
        const Json* objects = m.contains("layers") && m["layers"].is_object() &&
            m["layers"].contains("objects") ? &m["layers"]["objects"] : nullptr;
        const Json* above = m.contains("layers") && m["layers"].is_object() &&
            m["layers"].contains("above") ? &m["layers"]["above"] : nullptr;
        const auto tile_at = [this](const Json* layer, int x, int y) {
            if (!layer || !layer->is_array() ||
                y < 0 || y >= static_cast<int>(layer->as_array().size())) return -1;
            if (wraps_x) x = wrapX(x, W);
            else if (x < 0 || x >= W) return -1;
            const Json& row = (*layer)[static_cast<size_t>(y)];
            if (!row.is_array() || x < 0 ||
                x >= static_cast<int>(row.as_array().size())) return -1;
            return row[static_cast<size_t>(x)].as_int();
        };
        const auto tileset_it = game->project->tilesets.find(ts_id);
        const Json* tileset = tileset_it == game->project->tilesets.end()
            ? nullptr : &tileset_it->second;
        const std::map<int, std::string> groups = tileset ? tile_groups(*tileset)
                                                         : std::map<int, std::string>{};
        int x0 = wraps_x
            ? static_cast<int>(std::floor(cam_x / field_tile_size)) - 2
            : std::max(0, static_cast<int>(cam_x / field_tile_size) - 2);
        int y0 = std::max(0, static_cast<int>(cam_y / field_tile_size) - 2);
        int x1 = wraps_x
            ? static_cast<int>(std::ceil((cam_x + r.w) / field_tile_size)) + 3
            : std::min(W, static_cast<int>((cam_x + r.w) / field_tile_size) + 3);
        int y1 = std::min(H, static_cast<int>((cam_y + r.h) / field_tile_size) + 3);
        for (int ty = y0; ty < y1; ++ty) {
            for (int tx = x0; tx < x1; ++tx) {
                int gi = tile_at(ground, tx, ty);
                if (gi >= 0) {
                    if (tileset && groups.contains(gi) && groups.at(gi) == "ocean") {
                        const int mask = mask_at(m, *tileset, "ground", tx, ty, "ocean", groups);
                        const auto& ocean_tiles = (*tileset)["autotiles"]["ocean"]["tiles"].as_array();
                        if (mask >= 0 && static_cast<size_t>(mask) < ocean_tiles.size()) {
                            gi = ocean_tiles[static_cast<size_t>(mask)].as_int();
                        }
                    }
                    r.tile(ts_id, gi, tx * field_tile_size, ty * field_tile_size, "ground");
                }
                const int oi = tile_at(objects, tx, ty);
                if (oi >= 0) r.tile(ts_id, oi, tx * field_tile_size, ty * field_tile_size, "objects");
            }
        }
        std::string direction = hero.dir;
        const int walk_phase = static_cast<int>(hero.t * 8.0) % 4;
        const int walk_step = !hero.moving ? 0 : walk_phase == 0 ? 1 :
                              walk_phase == 2 ? 2 : 0;
        std::string frame = direction + "_" + std::to_string(walk_step);
        bool flip = false;
        const auto graphic_it = game->project->graphics.find("hero");
        if (graphic_it != game->project->graphics.end() &&
            graphic_it->second.contains("frames") && graphic_it->second["frames"].is_object()) {
            const Json& frames = graphic_it->second["frames"];
            if (!frames.contains(frame)) {
                std::string mirrored_direction = direction;
                if (direction == "left") { mirrored_direction = "right"; flip = true; }
                else if (direction == "up_left") { mirrored_direction = "up_right"; flip = true; }
                else if (direction == "down_left") { mirrored_direction = "down_right"; flip = true; }
                frame = mirrored_direction + "_" + std::to_string(walk_step);
            }
            if (!frames.contains(frame)) {
                frame = "idle_" + direction;
                if (!frames.contains(frame)) {
                    if (direction.find("up") != std::string::npos) frame = "idle_up";
                    else if (direction.find("left") != std::string::npos ||
                             direction.find("right") != std::string::npos) frame = "idle_right";
                    else frame = "idle_down";
                }
            }
        } else {
            frame = "idle_down";
        }
        int sprite_width = 32;
        int sprite_height = 48;
        if (graphic_it != game->project->graphics.end()) {
            if (graphic_it->second.contains("w")) sprite_width = graphic_it->second["w"].as_int();
            if (graphic_it->second.contains("h")) sprite_height = graphic_it->second["h"].as_int();
        }
        r.sprite("hero", frame, static_cast<int>(hero.x - sprite_width / 2.0),
                 static_cast<int>(hero.y - sprite_height), flip, 1.0, "", 1.0, true);
        for (int ty = y0; ty < y1; ++ty) {
            for (int tx = x0; tx < x1; ++tx) {
                const int ai = tile_at(above, tx, ty);
                if (ai >= 0) r.tile(ts_id, ai, tx * field_tile_size, ty * field_tile_size, "above");
            }
        }
    }
};

class TacticsScene : public Scene {
public:
    Game* game;
    Json m;
    TacticsScene(Game* game, const Json& m_, const std::string& = "", const std::vector<int>& = {}, const std::string& = "")
        : game(game), m(m_) {}
    void update(double, InputState&) override {}
    void draw(Renderer&) override {}
};

} // namespace aur
