#pragma once

// TerrainLod.hpp - which picture of a place to draw.
//
// The world map has two pictures of the same terrain: the generated HD
// surface, and that same terrain at one pixel per tile. The zoom decides
// which is worth drawing: close in, a tile covers enough screen that the
// fine picture earns its keep; far out, the coarse one is enough.

#include "aur/json.hpp"

namespace aur
{
    namespace world
    {

        // How many screen pixels a tile must cover before the HD surface is
        // drawn instead of the coarse picture.
        constexpr double kHdSurfacePixelsPerTile = 3.0;

        // The screen pixels a tile covers when the map is drawn `map_pixels`
        // wide on a map `map_width` tiles across.
        double pixelsPerTile(double map_pixels, int map_width);

        // The choice: the HD surface only when the map has one and its tiles
        // cover enough screen.
        bool useHdSurface(const Json &map, double map_pixels);

    } // namespace world
} // namespace aur
