#pragma once

#include "json.hpp"
#include "game.hpp"
#include <vector>
#include <string>

namespace aur {

class TacticsScene : public Scene {
public:
    Game* game;
    Json m;
    int W = 0, H = 0;
    std::vector<std::vector<int>> flags;
    struct Unit { int x = 0, y = 0; int hp = 10, max_hp = 10; std::string team = "player"; bool moved = false, acted = false; };
    std::vector<Unit> units;
    int turn = 0;
    int selected = -1;
    double t = 0.0;

    TacticsScene(Game* game, const Json& m_, const std::string& = "", const std::vector<int>& = {}, const std::string& = "")
        : game(game), m(m_) {
        W = m["w"].as_int(); H = m["h"].as_int();
        flags = game->project->collision(m);
    }

    void update(double dt, InputState& inp) override {
        t += dt;
        // advance turn skeleton
        if (inp.pressed.count("confirm")) {
            if (selected >= 0 && selected < (int)units.size()) {
                units[selected].moved = true;
                units[selected].acted = true;
            }
            selected = -1;
        }
        if (inp.pressed.count("cancel")) selected = -1;
    }

    void draw(Renderer& r) override {
        r.camera(0, 0);
        const auto& ground = m["layers"]["ground"].as_array();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int gi = ground[y].as_array()[x].as_int();
                r.tile(m["tileset"].as_string(), gi, x * 16, y * 16, "ground");
            }
        for (auto& u : units) {
            r.ui_rect(u.x * 16, u.y * 16, 14, 14, u.team == "player" ? "blue" : "red", 0.8);
        }
        if (selected >= 0 && selected < (int)units.size()) {
            auto& u = units[selected];
            r.ring(u.x * 16 + 7, u.y * 16 + 7, 10, {255, 255, 100}, 0.9);
        }
    }
};

} // namespace aur
