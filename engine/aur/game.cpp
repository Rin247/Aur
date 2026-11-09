#include "game.hpp"
#include "field.hpp"
#include "world_topology.hpp"
#include <algorithm>
#include <cstdio>

namespace aur {

void Game::update(double dt, InputState& inp) {
    real_time += dt;
    clock.update(dt);
    if (!started) {
        started = true;
        std::string start_map = project && project->meta.contains("start") && project->meta["start"].contains("map")
            ? project->meta["start"]["map"].as_string() : "";
        if (!start_map.empty() && project && project->maps.contains(start_map)) {
            map_id = start_map;
            scenes.push_back(std::make_unique<FieldScene>(this, project->maps[start_map],
                project->meta.contains("start") && project->meta["start"].contains("spawn")
                    ? project->meta["start"]["spawn"].as_string() : "",
                std::vector<int>{}, ""));
        }
    }
    if (!climate_override && project && project->maps.contains(map_id)) {
        const Json& active_map = project->maps[map_id];
        std::string climate = active_map["climate"].is_string()
            ? active_map["climate"].as_string() : active_climate;
        const FieldScene* field = scenes.empty() ? nullptr
            : dynamic_cast<const FieldScene*>(scenes.back().get());
        if (field && active_map["layers"].contains("biomes") &&
            active_map["layers"]["biomes"].is_array()) {
            const int raw_tile_x = static_cast<int>(field->hero.x / field_tile_size);
            const int tile_x = wraps_world_x(active_map)
                ? wrapX(raw_tile_x, active_map["w"].as_int())
                : std::clamp(raw_tile_x, 0, active_map["w"].as_int() - 1);
            const int tile_y = std::clamp(static_cast<int>(field->hero.y / field_tile_size),
                                          0, active_map["h"].as_int() - 1);
            const JsonArray& biome_rows = active_map["layers"]["biomes"].as_array();
            if (tile_y < static_cast<int>(biome_rows.size()) &&
                biome_rows[static_cast<size_t>(tile_y)].is_array() &&
                tile_x < static_cast<int>(biome_rows[static_cast<size_t>(tile_y)].as_array().size())) {
                const std::string biome_id = biome_rows[static_cast<size_t>(tile_y)]
                    [static_cast<size_t>(tile_x)].as_string();
                const auto biome = project->biomes.find(biome_id);
                if (biome != project->biomes.end() && biome->second["climate"].is_string()) {
                    climate = biome->second["climate"].as_string();
                }
            }
        }
        if (climate != active_climate) {
            active_climate = climate;
            if (!weather.manual_override) weather.slot_cache = {-1, -1};
        }
    }
    weather.update(dt, clock, active_climate, project ? project->meta["weather"] : Json{});
    if (weather.current() == "storm" && real_time > dt && std::fmod(real_time, 13.0) < dt) {
        weather.flash = 1.0;
    }
    if (script && !script->done) {
        script->update(dt);
    }
    for (const auto& scene : scenes) {
        scene->update(dt, inp);
    }
    inp.end_frame();
}

void Game::draw(Renderer& r) {
    r.begin("hd2d");
    for (const auto& s : scenes) {
        s->draw(r);
    }
    const auto [ambient_rgb, darkness] = clock.ambient();
    char ambient_color[8];
    std::snprintf(ambient_color, sizeof(ambient_color), "#%02x%02x%02x",
                  std::get<0>(ambient_rgb), std::get<1>(ambient_rgb),
                  std::get<2>(ambient_rgb));
    r.ambient(ambient_color, darkness);
    r.weather(weather.current(), weather.intensity, real_time, weather.flash);
    r.end();
}

void Game::refreshProjectSettings() {
    if (!project) return;
    const int old_year = clock.year();
    const std::string old_season = clock.season();
    const int old_day = clock.day_of_season();
    const int old_hour = clock.hour();
    const int old_minute = clock.minute();
    const bool was_paused = clock.paused;

    GameClock updated = project_clock(project);
    auto season = std::find(updated.configured_seasons.begin(),
                            updated.configured_seasons.end(), old_season);
    int season_index = season == updated.configured_seasons.end()
        ? std::min(clock.season_index(), static_cast<int>(updated.configured_seasons.size()) - 1)
        : static_cast<int>(std::distance(updated.configured_seasons.begin(), season));
    updated.set_date_time(std::max(1, old_year), season_index,
                          std::clamp(old_day, 1, updated.configured_days_per_season),
                          std::min(old_hour, static_cast<int>(updated.day_minutes / 60.0) - 1),
                          old_minute);
    updated.paused = was_paused;
    clock = std::move(updated);

    const Json& eras = project->meta["eras"];
    if (!eras.is_object() || eras.as_object().empty()) return;
    if (!eras.contains(state["current_era"].as_string())) {
        state["current_era"] = eras.as_object().begin()->first;
    }
}

void Game::travel(const std::string& map_id, const std::string& spawn, const std::vector<int>& pos, const std::string& dir, const std::string& effect) {
    if (!project || !project->maps.contains(map_id)) return;
    this->map_id = map_id;
    if (!climate_override && project->maps[map_id]["climate"].is_string()) {
        active_climate = project->maps[map_id]["climate"].as_string();
    }
    scenes.clear();
    scenes.push_back(std::make_unique<FieldScene>(this, project->maps[map_id], spawn, pos, dir));
}

} // namespace aur
