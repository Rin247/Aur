#include "TerrainLod.hpp"

#include "WorldMapHelpers.hpp"

#include <algorithm>
#include <cmath>

namespace aur
{
    namespace world
    {

        bool CloseRegionKey::contains(const std::string &map, std::uint64_t rev,
                                      const QRect &visible_tiles, int target_density) const
        {
            if (map_id != map || revision != rev || density != target_density) return false;
            if (tile_bounds.isEmpty() || visible_tiles.isEmpty()) return false;
            return tile_bounds.left() <= visible_tiles.left() &&
                   tile_bounds.top() <= visible_tiles.top() &&
                   tile_bounds.left() + tile_bounds.width() >=
                       visible_tiles.left() + visible_tiles.width() &&
                   tile_bounds.top() + tile_bounds.height() >=
                       visible_tiles.top() + visible_tiles.height();
        }

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

        LodTier selectLodTier(const Json &map, double map_pixels)
        {
            if (!useHdSurface(map, map_pixels)) return LodTier::Coarse;
            const double ppt = pixelsPerTile(map_pixels, map["w"].as_int());
            return ppt > kCloseRegionPixelsPerTile ? LodTier::CloseRegion : LodTier::Hd;
        }

        int closeRegionDensity(double pixels_per_tile)
        {
            if (pixels_per_tile <= kCloseRegionPixelsPerTile) return 32;
            return pixels_per_tile <= 64.0 ? 64 : 128;
        }

        QRect closeRegionTileBounds(const QRectF &map_bounds, const QSize &viewport_size,
                                    int map_width, int map_height, bool wraps_x,
                                    int margin_tiles)
        {
            if (map_width <= 0 || map_height <= 0 || map_bounds.width() <= 0.0 ||
                map_bounds.height() <= 0.0 || viewport_size.isEmpty()) {
                return {};
            }
            const double cell_w = map_bounds.width() / map_width;
            const double cell_h = map_bounds.height() / map_height;
            int x0 = static_cast<int>(std::floor((0.0 - map_bounds.left()) / cell_w)) - margin_tiles;
            int x1 = static_cast<int>(
                         std::ceil((viewport_size.width() - map_bounds.left()) / cell_w)) +
                     margin_tiles;
            const int y0 = std::clamp(
                static_cast<int>(std::floor((0.0 - map_bounds.top()) / cell_h)) - margin_tiles,
                0, map_height);
            const int y1 = std::clamp(
                static_cast<int>(std::ceil((viewport_size.height() - map_bounds.top()) / cell_h)) +
                    margin_tiles,
                y0, map_height);
            if (!wraps_x) {
                x0 = std::clamp(x0, 0, map_width);
                x1 = std::clamp(x1, x0, map_width);
            } else if (x1 - x0 >= map_width) {
                x0 = 0;
                x1 = map_width;
            }
            return QRect(x0, y0, x1 - x0, y1 - y0);
        }

        QRect globeCloseRegionTileBounds(double yaw, double pitch, double pixels_per_tile,
                                         const QSize &viewport_size, int map_width,
                                         int map_height, int margin_tiles)
        {
            if (map_width <= 0 || map_height <= 0 || pixels_per_tile <= 0.0 ||
                viewport_size.isEmpty()) {
                return {};
            }
            constexpr double kPi = 3.14159265358979323846;
            const double cp = std::cos(pitch);
            const double ox = -cp * std::sin(yaw);
            const double oz = cp * std::cos(yaw);
            double u = std::fmod(std::atan2(ox, oz) / (2.0 * kPi) + 0.5, 1.0);
            if (u < 0.0) u += 1.0;
            const double v = std::clamp(
                0.5 - std::asin(std::clamp(std::sin(pitch), -1.0, 1.0)) / kPi, 0.0, 1.0);
            const int cx = static_cast<int>(std::floor(u * map_width));
            const int cy = static_cast<int>(std::floor(v * map_height));
            const double lat_scale = std::max(0.25, std::abs(cp));
            const int span_x = std::clamp(
                static_cast<int>(std::ceil(viewport_size.width() / (pixels_per_tile * lat_scale))) +
                    2 * margin_tiles,
                4, map_width);
            const int span_y = std::clamp(
                static_cast<int>(std::ceil(viewport_size.height() / pixels_per_tile)) +
                    2 * margin_tiles,
                4, map_height);
            const int x0 = cx - span_x / 2;
            const int y0 = std::clamp(cy - span_y / 2, 0, std::max(0, map_height - span_y));
            const int y1 = std::clamp(y0 + span_y, y0, map_height);
            return QRect(x0, y0, span_x, y1 - y0);
        }

    } // namespace world
} // namespace aur
