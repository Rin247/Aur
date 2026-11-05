#include "mini_test.hpp"
#include "aur/clock.hpp"
#include "aur/project.hpp"
#include "aur/game.hpp"

void test_project_calendar_and_weather() {
    aur::Project project;
    project.meta["calendar"]["real_day_minutes"] = 48;
    project.meta["calendar"]["start_hour"] = 3;
    aur::GameClock clock = aur::project_clock(&project);
    ASSERT(clock.hour() == 3);
    ASSERT(clock.period() == "dawn");
    clock.update(48.0 * 60.0);
    ASSERT(clock.day() == 1);
    ASSERT(clock.hour() == 3);

    clock.set_date_time(2, 1, 3, 7, 30);
    ASSERT(clock.year() == 2);
    ASSERT(clock.season() == "summer");
    ASSERT(clock.day_of_season() == 3);
    ASSERT(clock.hour() == 7);
    ASSERT(clock.minute() == 30);

    aur::Json settings = aur::parse_json(
        R"({"interval_hours":3,"profiles":{"temperate":{"snow":100}}})");
    aur::WeatherSystem weather(42);
    weather.roll(clock, "temperate", settings);
    ASSERT(weather.target == "snow");
    weather.force("rain");
    weather.update(0.0, clock, "temperate", settings);
    ASSERT(weather.target == "rain");
    weather.clear_override();
    weather.roll(clock, "temperate", settings);
    ASSERT(weather.target == "snow");
}
