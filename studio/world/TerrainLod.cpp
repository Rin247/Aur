#include "TerrainLod.hpp"

#include "WorldMapHelpers.hpp"

namespace aur
{
    namespace world
    {

        double pixelsPerTile(double map_pixels, int map_width)
        {
            if (map_width <= 0) return 0.0;
            return map_pixels / static_cast<double>(map_width);
        }

        bool useHdSurface(const Json &map, double map_pixels)
        {
            if (!has_hd_surface(map)) return false;
            if (!map.contains("w") || !map["w"].is_number()) return false;
            return pixelsPerTile(map_pixels, map["w"].as_int()) >= kHdSurfacePixelsPerTile;
        }

    } // namespace world
} // namespace aur
