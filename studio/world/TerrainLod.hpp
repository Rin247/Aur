#pragma once

#include "aur/json.hpp"

#include <QRect>
#include <QRectF>
#include <QSize>
#include <cstdint>
#include <string>

namespace aur
{
    namespace world
    {

        constexpr double kHdSurfacePixelsPerTile = 3.0;
        constexpr double kCloseRegionPixelsPerTile = 32.0;

        enum class LodTier
        {
            Coarse,
            Hd,
            CloseRegion
        };

        struct CloseRegionKey
        {
            std::string map_id;
            std::uint64_t revision = 0;
            QRect tile_bounds;
            int density = 0;

            bool operator==(const CloseRegionKey &) const = default;
            bool contains(const std::string &map, std::uint64_t rev,
                          const QRect &visible_tiles, int target_density) const;
        };

        double pixelsPerTile(double map_pixels, int map_width);
        bool useHdSurface(const Json &map, double map_pixels);
        LodTier selectLodTier(const Json &map, double map_pixels);
        int closeRegionDensity(double pixels_per_tile);
        QRect closeRegionTileBounds(const QRectF &map_bounds, const QSize &viewport_size,
                                    int map_width, int map_height, bool wraps_x,
                                    int margin_tiles = 4);
        QRect globeCloseRegionTileBounds(double yaw, double pitch, double pixels_per_tile,
                                         const QSize &viewport_size, int map_width,
                                         int map_height, int margin_tiles = 4);

    } // namespace world
} // namespace aur
