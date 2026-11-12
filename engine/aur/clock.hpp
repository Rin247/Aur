#pragma once

#include "json.hpp"
#include <string>
#include <vector>
#include <tuple>
#include <random>
#include <cmath>

namespace aur {

const std::vector<std::string> SEASONS = {"spring", "summer", "autumn", "winter"};
constexpr int DAYS_PER_SEASON = 28;

class GameClock {
public:
    double day_minutes = 1440.0;
    int configured_days_per_season = DAYS_PER_SEASON;
    std::vector<std::string> configured_seasons = SEASONS;
    std::vector<double> phase_hours = {6.0, 12.0, 18.0};
    double minutes;
    double scale;
    bool paused = false;

    explicit GameClock(double start_hour = 8.0, double scale = 1.0,
                       double day_minutes = 1440.0, int days_per_season = DAYS_PER_SEASON,
                       std::vector<std::string> seasons = SEASONS,
                       std::vector<double> phase_hours = {6.0, 12.0, 18.0})
        : day_minutes(day_minutes), configured_days_per_season(days_per_season),
          configured_seasons(std::move(seasons)), phase_hours(std::move(phase_hours)),
          minutes(start_hour * 60.0), scale(scale) {}

    void update(double dt) {
        if (!paused) {
            minutes += dt * scale;
        }
    }

    void advance(double hours) {
        minutes += hours * 60.0;
    }

    void set_time(double hour, double minute = 0.0) {
        minutes = day() * day_minutes + (hour + minute / 60.0) * 60.0;
    }

    void set_date_time(int year, int season, int day_of_season, double hour, double minute) {
        const int days_per_year = configured_days_per_season *
                                  static_cast<int>(configured_seasons.size());
        const int absolute_day = (year - 1) * days_per_year +
                                 season * configured_days_per_season + day_of_season - 1;
        minutes = absolute_day * day_minutes + (hour + minute / 60.0) * 60.0;
    }

    int day() const { return static_cast<int>(minutes / day_minutes); }
    double hour_f() const { return std::fmod(minutes, day_minutes) / 60.0; }
    int hour() const { return static_cast<int>(hour_f()); }
    int minute() const { return static_cast<int>(std::fmod(hour_f(), 1.0) * 60.0); }
    int season_index() const {
        return (day() / configured_days_per_season) % static_cast<int>(configured_seasons.size());
    }
    std::string season() const { return configured_seasons[static_cast<size_t>(season_index())]; }
    int day_of_season() const { return (day() % configured_days_per_season) + 1; }
    int year() const {
        return day() / (configured_days_per_season * static_cast<int>(configured_seasons.size())) + 1;
    }

    bool between(double a, double b) const {
        double h = hour_f();
        if (a <= b) return h >= a && h < b;
        return h >= a || h < b;
    }

    bool is_night() const { return between(phase_hours[2], phase_hours[0]); }

    std::string period() const {
        const int phase = hour_f() < phase_hours[0] ? 0
                        : hour_f() < phase_hours[1] ? 1
                        : hour_f() < phase_hours[2] ? 2 : 3;
        static const std::vector<std::string> names = {"dawn", "day", "dusk", "night"};
        return names[static_cast<size_t>(phase)];
    }

    std::tuple<std::tuple<int,int,int>, double> ambient() const;
    std::string label() const;

    Json to_dict() const {
        JsonObject d;
        d["minutes"] = minutes;
        d["scale"] = scale;
        return d;
    }

    static GameClock from_dict(const Json& d) {
        GameClock c;
        if (d.contains("scale")) c.scale = d["scale"].as_number();
        if (d.contains("minutes")) c.minutes = d["minutes"].as_number();
        return c;
    }
};

// The weather a climate's profile gives at a slot: the same pick the
// runtime makes, so the editor's preview and the game agree. The slot is
// the clock's day and the hour divided by the interval.
std::string weather_for_slot(const Json &settings, const std::string &climate,
                             int seed, int day, int slot_hour);

// The weather's seed when nobody names one. The game and the editor
// share it, so the preview and the game roll the same weather.
constexpr int DEFAULT_WEATHER_SEED = 1;

class WeatherSystem {
public:
    int seed;
    int interval;
    std::string kind;
    std::string target;
    double intensity;
    double flash;
    std::pair<int, int> slot_cache = {-1, -1};
    bool manual_override = false;

    explicit WeatherSystem(int seed = 1, int interval = 3)
        : seed(seed), interval(interval), kind("clear"), target("clear"), intensity(1.0), flash(0.0) {}

    void roll(const GameClock& clock, const std::string& climate = "temperate");
    void roll(const GameClock& clock, const std::string& climate, const Json& settings);
    void update(double dt, const GameClock& clock, const std::string& climate = "temperate",
                const Json& settings = Json{});
    void force(const std::string& k);
    void clear_override() { manual_override = false; slot_cache = {-1, -1}; }
    std::string current() const { return intensity >= 0.5 ? target : kind; }
    double darkness() const;

    Json to_dict() const {
        JsonObject d;
        d["kind"] = kind;
        d["target"] = target;
        d["intensity"] = intensity;
        d["seed"] = seed;
        return d;
    }

    static WeatherSystem from_dict(const Json& d) {
        WeatherSystem w(d.contains("seed") ? d["seed"].as_int() : 1);
        if (d.contains("kind")) w.kind = d["kind"].as_string();
        if (d.contains("target")) w.target = d["target"].as_string();
        if (d.contains("intensity")) w.intensity = d["intensity"].as_number();
        return w;
    }
};

inline Json schedule_pick(const JsonArray& entries, const GameClock& clock) {
    for (const auto& e : entries) {
        double from_h = e.contains("from") ? e["from"].as_number() : 0.0;
        double to_h = e.contains("to") ? e["to"].as_number() : 24.0;
        if (clock.between(from_h, to_h)) {
            if (e.contains("season") && e["season"].as_string() != clock.season()) {
                continue;
            }
            return e;
        }
    }
    return nullptr;
}

} // namespace aur
