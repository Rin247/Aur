#pragma once

#include "json.hpp"
#include "project.hpp"
#include "clock.hpp"
#include "render.hpp"
#include "script.hpp"
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <algorithm>
#include <cmath>

namespace aur {

inline GameClock project_clock(const Project* project) {
    if (!project) return GameClock{};
    const Json& calendar = project->meta.at("calendar");
    double hours_per_day = calendar["hours_per_day"].is_number()
        ? calendar["hours_per_day"].as_number() : 24.0;
    if (!std::isfinite(hours_per_day) || hours_per_day < 1.0 || hours_per_day > 48.0) {
        hours_per_day = 24.0;
    }
    double real_day_minutes = calendar["real_day_minutes"].is_number()
        ? calendar["real_day_minutes"].as_number() : 48.0;
    if (!std::isfinite(real_day_minutes) || real_day_minutes < 1.0 || real_day_minutes > 10080.0) {
        real_day_minutes = 48.0;
    }
    const double day_minutes = hours_per_day * 60.0;
    const double scale = day_minutes / (real_day_minutes * 60.0);
    double start_hour = calendar["start_hour"].is_number()
        ? calendar["start_hour"].as_number()
        : (project->meta.at("start_hour").is_number() ? project->meta.at("start_hour").as_number() : 8.0);
    if (!std::isfinite(start_hour) || start_hour < 0.0 || start_hour >= hours_per_day) {
        start_hour = std::min(8.0, hours_per_day - 0.01);
    }
    int days_per_season = calendar["days_per_season"].is_number()
        ? calendar["days_per_season"].as_int() : DAYS_PER_SEASON;
    if (days_per_season < 1 || days_per_season > 365) days_per_season = DAYS_PER_SEASON;

    std::vector<std::string> seasons;
    if (calendar["seasons"].is_array()) {
        for (const Json& season : calendar["seasons"].as_array()) {
            if (season.is_string()) seasons.push_back(season.as_string());
        }
    }
    if (seasons.empty()) seasons = SEASONS;

    std::vector<double> phase_hours;
    if (calendar["phase_hours"].is_array()) {
        for (const Json& hour : calendar["phase_hours"].as_array()) {
            if (hour.is_number()) phase_hours.push_back(hour.as_number());
        }
    }
    if (phase_hours.size() != 3 || !std::isfinite(phase_hours[0]) ||
        !std::isfinite(phase_hours[1]) || !std::isfinite(phase_hours[2]) ||
        phase_hours[0] <= 0.0 || phase_hours[0] >= phase_hours[1] ||
        phase_hours[1] >= phase_hours[2] || phase_hours[2] >= hours_per_day) {
        phase_hours = {hours_per_day * 0.25, hours_per_day * 0.5, hours_per_day * 0.75};
    }
    return GameClock(start_hour, scale, day_minutes, days_per_season,
                     std::move(seasons), std::move(phase_hours));
}

class InputState {
public:
    std::set<std::string> held;
    std::set<std::string> pressed;

    void press(const std::string& name) {
        held.insert(name);
        pressed.insert(name);
    }
    void release(const std::string& name) {
        held.erase(name);
    }
    void end_frame() {
        pressed.clear();
    }
};

class Scene {
public:
    bool opaque = true;
    virtual ~Scene() = default;
    virtual void update(double dt, InputState& inp) {}
    virtual void draw(Renderer& r) {}
};

class Game {
public:
    Project* project;
    GameClock clock;
    WeatherSystem weather;
    JsonObject state;
    std::vector<std::unique_ptr<Scene>> scenes;
    std::unique_ptr<Script> script;
    std::vector<JsonArray> scripts_queue;
    std::string map_id;
    std::string active_climate = "temperate";
    bool climate_override = false;
    double real_time = 0.0;
    Renderer layout;

    explicit Game(Project* project, int seed = DEFAULT_WEATHER_SEED)
        : project(project), clock(project_clock(project)), weather(seed) {
        state["switches"] = JsonObject{};
        state["vars"] = JsonObject{};
        state["current_era"] = project && project->meta["current_era"].is_string()
            ? project->meta["current_era"].as_string() : "primordial_dawn";
        state["inventory"] = JsonObject{};
        state["gold"] = 0;
        state["party"] = JsonArray{};
        state["actors"] = JsonObject{};
    }

    void update(double dt, InputState& inp);
    void draw(Renderer& r);
    // Starts on a named map instead of the project's start map.
    void startAt(const std::string& map_id);
    void travel(const std::string& map_id, const std::string& spawn, const std::vector<int>& pos, const std::string& dir, const std::string& effect = "fade");
    void refreshProjectSettings();

private:
    bool started = false;
};

} // namespace aur
