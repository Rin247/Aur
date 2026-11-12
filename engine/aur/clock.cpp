#include "clock.hpp"
#include <algorithm>
#include <cmath>

namespace aur {

static const std::vector<std::tuple<double, std::tuple<int,int,int>, double>> LIGHT_KEYS = {
    {0.0, {40, 55, 120}, 0.55},
    {5.0, {60, 70, 140}, 0.45},
    {6.5, {255, 175, 125}, 0.18},
    {8.0, {255, 255, 255}, 0.0},
    {17.0, {255, 255, 255}, 0.0},
    {18.5, {255, 160, 110}, 0.14},
    {20.0, {120, 105, 175}, 0.38},
    {22.0, {50, 60, 125}, 0.52},
    {24.0, {40, 55, 120}, 0.55}
};

std::tuple<std::tuple<int,int,int>, double> GameClock::ambient() const {
    double h = hour_f();
    for (size_t i = 0; i + 1 < LIGHT_KEYS.size(); ++i) {
        auto [h0, c0, d0] = LIGHT_KEYS[i];
        auto [h1, c1, d1] = LIGHT_KEYS[i+1];
        if (h0 <= h && h <= h1) {
            double t = (h1 > h0) ? (h - h0) / (h1 - h0) : 0.0;
            int r = static_cast<int>(std::get<0>(c0) + (std::get<0>(c1) - std::get<0>(c0)) * t);
            int g = static_cast<int>(std::get<1>(c0) + (std::get<1>(c1) - std::get<1>(c0)) * t);
            int b = static_cast<int>(std::get<2>(c0) + (std::get<2>(c1) - std::get<2>(c0)) * t);
            double d = d0 + (d1 - d0) * t;
            return {{r, g, b}, d};
        }
    }
    return {std::get<1>(LIGHT_KEYS.back()), std::get<2>(LIGHT_KEYS.back())};
}

std::string GameClock::label() const {
    char buf[64];
    std::string s_name = season();
    s_name[0] = std::toupper(s_name[0]);
    std::snprintf(buf, sizeof(buf), "%s %d, %02d:%02d", s_name.c_str(), day_of_season(), hour(), minute());
    return std::string(buf);
}

void WeatherSystem::roll(const GameClock& clock, const std::string& climate) {
    roll(clock, climate, Json{});
}

std::string weather_for_slot(const Json& settings, const std::string& climate,
                             int seed, int day, int slot_hour) {
    std::mt19937 rng(seed + day * 100 + slot_hour);
    std::string new_weather = "clear";
    const Json& profiles = settings["profiles"];
    const Json* profile = profiles.is_object() && profiles.contains(climate)
        ? &profiles[climate] : nullptr;
    if (!profile && profiles.is_object() && profiles.contains("temperate")) {
        profile = &profiles["temperate"];
    }
    if (profile && profile->is_object()) {
        int total_weight = 0;
        for (const auto& [state, weight] : profile->as_object()) {
            if (weight.is_number() && weight.as_int() > 0) total_weight += weight.as_int();
        }
        if (total_weight > 0) {
            int roll_value = static_cast<int>(rng() % static_cast<unsigned int>(total_weight));
            for (const auto& [state, weight] : profile->as_object()) {
                if (!weight.is_number() || weight.as_int() <= 0) continue;
                roll_value -= weight.as_int();
                if (roll_value < 0) {
                    new_weather = state;
                    break;
                }
            }
        }
    } else if (climate == "arid") {
        new_weather = (rng() % 10 == 0) ? "cloudy" : "clear";
    } else if (climate == "arctic") {
        new_weather = "snow";
    } else if (climate == "none") {
        new_weather = "clear";
    } else {
        int r = rng() % 10;
        if (r < 4) new_weather = "clear";
        else if (r < 7) new_weather = "cloudy";
        else new_weather = "rain";
    }
    return new_weather;
}

void WeatherSystem::roll(const GameClock& clock, const std::string& climate, const Json& settings) {
    if (settings.contains("interval_hours") && settings["interval_hours"].is_number()) {
        interval = std::max(1, settings["interval_hours"].as_int());
    }
    int slot_hour = clock.hour() / std::max(1, interval);
    std::pair<int, int> current_slot = {clock.day(), slot_hour};
    if (current_slot == slot_cache) return;
    slot_cache = current_slot;

    const std::string new_weather =
        weather_for_slot(settings, climate, seed, clock.day(), slot_hour);

    if (new_weather != target) {
        kind = target;
        target = new_weather;
        intensity = 0.0;
    }
}

void WeatherSystem::update(double dt, const GameClock& clock, const std::string& climate,
                           const Json& settings) {
    if (!manual_override) roll(clock, climate, settings);
    intensity = std::min(1.0, intensity + dt / 6.0);
    if (intensity >= 1.0) {
        kind = target;
    }
    flash = std::max(0.0, flash - dt * 2.5);
}

void WeatherSystem::force(const std::string& k) {
    kind = target = k;
    intensity = 1.0;
    manual_override = true;
}

double WeatherSystem::darkness() const {
    double a = (kind == "rain" || kind == "storm") ? 0.2 : 0.0;
    double b = (target == "rain" || target == "storm") ? 0.2 : 0.0;
    return a + (b - a) * intensity;
}

} // namespace aur
