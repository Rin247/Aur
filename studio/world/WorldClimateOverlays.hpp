#pragma once

#include "aur/json.hpp"
#include "aur/project.hpp"

#include <QPainter>
#include <QRectF>

namespace aur::world
{

    struct TemperatureOverlayContext
    {
        QPainter &painter;
        const Project *project = nullptr;
        const JsonArray *temperature_rows = nullptr;
        QRectF bounds;
        qreal cell = 0.0;
        int map_width = 0;
        int first_x = 0;
        int last_x = 0;
        int first_y = 0;
        int last_y = 0;
        bool wraps_x = false;
    };

    void draw_temperature_overlay(TemperatureOverlayContext &context);

    struct ClimateAxesContext
    {
        QPainter &painter;
        const Project *project = nullptr;
        const Json *map = nullptr;
        QRectF bounds;
        QRectF visible;
        qreal cell = 0.0;
        bool wraps_x = false;
    };

    void draw_climate_axes(ClimateAxesContext &context);

    struct FlowOverlayContext
    {
        QPainter &painter;
        const Project *project = nullptr;
        const Json *map = nullptr;
        QRectF bounds;
        qreal cell = 0.0;
        int map_width = 0;
        int map_height = 0;
        int first_x = 0;
        int last_x = 0;
        int first_y = 0;
        int last_y = 0;
        bool wraps_x = false;
        bool currents = false;
        int simulation_tick = 0;
        bool simulation_running = false;
        std::function<bool(int, int)> is_ocean_at;
    };

    void draw_flow_overlay(FlowOverlayContext &context);

    struct DayNightContext
    {
        QPainter &painter;
        const Project *project = nullptr;
        QRectF bounds;
        QRectF visible;
        bool wraps_x = false;
        double hour = 8.0;
    };

    void draw_daynight_fallback(DayNightContext &context);

}
