#pragma once

#include "json.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace aur {

class Game;

class WaitCondition {
public:
    virtual ~WaitCondition() = default;
    virtual bool update(double dt) = 0;
};

class TimerWait : public WaitCondition {
public:
    double t;
    explicit TimerWait(double seconds) : t(seconds) {}
    bool update(double dt) override {
        t -= dt;
        return t <= 0;
    }
};

class PredicateWait : public WaitCondition {
public:
    std::function<bool()> pred;
    explicit PredicateWait(std::function<bool()> p) : pred(std::move(p)) {}
    bool update(double dt) override {
        return pred();
    }
};

class Script {
public:
    Game* game;
    JsonArray cmds;
    size_t ip = 0;
    std::unique_ptr<WaitCondition> wait;
    bool done = false;

    Script(Game* game, JsonArray cmds) : game(game), cmds(std::move(cmds)) {}

    void update(double dt);
};

std::vector<std::string> validate_script(const JsonArray& cmds, const std::string& path = "event");

} // namespace aur
