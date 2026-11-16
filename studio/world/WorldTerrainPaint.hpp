#pragma once

// WorldTerrainPaint.hpp - one level of detail of a world map's terrain.
//
// Either a generated surface (the fine one, or the same terrain at a
// coarse resolution) drawn over the visible cells, or the classic
// per-tile ground layer.

#include "aur/json.hpp"

#include <QImage>
#include <QPainter>
#include <QRectF>

#include <functional>
#include <map>
#include <string>

namespace aur
{
    namespace world
    {

        struct TerrainPaintContext
        {
            QPainter &painter;
            const Json *map = nullptr;
            QRectF bounds;
            QRectF visible;
            qreal cell = 0.0;
            int map_width = 0;
            int map_height = 0;
            int first_x = 0;
            int last_x = 0;
            int first_y = 0;
            int last_y = 0;
            bool wraps_x = false;
            bool repeat_x = false;
            QImage surface;
            const JsonArray *ground = nullptr;
            const std::map<int, std::string> *groups = nullptr;
            const Json *tileset = nullptr;
            std::string tileset_id;
            std::function<QImage(int)> tile_image;
        };

        void draw_terrain(TerrainPaintContext &context);

    } // namespace world
} // namespace aur
