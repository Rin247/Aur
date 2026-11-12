#pragma once

#include "aur/json.hpp"
#include "aur/project.hpp"

#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <functional>
#include <string>

namespace aur::world {

struct WeatherOverlayContext {
    QPainter& painter;
    const Project* project = nullptr;
    const Json* map = nullptr;  // WP3: preview field source
    const JsonArray* temperature_rows = nullptr;
    const JsonArray* biome_rows = nullptr;
    QRectF bounds;
    QRectF visible;
    qreal cell = 0.0;
    int map_width = 0;
    int map_height = 0;
    int first_x = 0;
    int last_x = 0;
    bool wraps_x = false;
    int worldgen_seed = 1;
    int simulation_tick = 0;
    bool simulation_running = false;
    double hour = 8.0;
    std::string weather = "climate";
    std::function<bool(int, int)> is_ocean_at;
};

void draw_weather_overlay(WeatherOverlayContext& context);

}


