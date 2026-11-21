#include "mini_test.hpp"
#include "world/HdSurface.hpp"
#include "world/TerrainLod.hpp"
#include "world/WorldMapHelpers.hpp"
#include "aur/project.hpp"

void test_terrain_lod()
{
    using namespace aur;

    Project project;
    const Json map = generate_world_map("lod_world", "Lod World", 32, 16, 24681357,
                                        "continents", 50, 60, 10);
    ASSERT(world::has_hd_surface(map));

    // The measurement is the drawn width over the map's width.
    ASSERT(world::pixelsPerTile(1024.0, 32) == 32.0);
    ASSERT(world::pixelsPerTile(0.0, 32) == 0.0);
    ASSERT(world::pixelsPerTile(1024.0, 0) == 0.0);

    // Below the crossover the coarse picture; at it and above, the HD one.
    const double crossover = world::kHdSurfacePixelsPerTile * 32.0;
    ASSERT(!world::useHdSurface(map, crossover - 1.0));
    ASSERT(world::useHdSurface(map, crossover));
    ASSERT(world::useHdSurface(map, crossover * 4.0));

    // A map with no HD surface never draws one, whatever the zoom.
    Json plain = Json(JsonObject{});
    plain["w"] = 32;
    plain["h"] = 16;
    ASSERT(!world::useHdSurface(plain, 1000000.0));

    // The two pictures of the same terrain: the fine surface, and the same
    // terrain at one pixel per tile.
    const QImage fine = world::build_hd_surface(map, &project, QRect(), 0);
    const QImage coarse = world::build_hd_surface(map, &project, QRect(), 1);
    ASSERT(fine.size() == QSize(1024, 512));
    ASSERT(coarse.size() == QSize(32, 16));

    // P3 (W-04): three LOD tiers and power-of-two close-region density.
    ASSERT(world::selectLodTier(map, crossover - 1.0) == world::LodTier::Coarse);
    ASSERT(world::selectLodTier(map, crossover) == world::LodTier::Hd);
    ASSERT(world::selectLodTier(map, 32.0 * 32.0) == world::LodTier::Hd);
    ASSERT(world::selectLodTier(map, 32.0 * 32.0 + 1.0) == world::LodTier::CloseRegion);
    ASSERT(world::selectLodTier(plain, 1000000.0) == world::LodTier::Coarse);
    ASSERT(world::closeRegionDensity(16.0) == 32);
    ASSERT(world::closeRegionDensity(48.0) == 64);
    ASSERT(world::closeRegionDensity(96.0) == 128);

    // Region-scoped build_hd_surface renders only the requested tile bounds
    // and supports 64/128 px/tile close-up densities plus wrapped X regions.
    const QRect sub_region(6, 3, 12, 8);
    const QImage sub_coarse = world::build_hd_surface(map, &project, sub_region, 1);
    ASSERT(sub_coarse == coarse.copy(sub_region));
    const QImage sub_dense = world::build_hd_surface(map, &project, QRect(6, 3, 8, 6), 64);
    ASSERT(sub_dense.size() == QSize(512, 384));
    const QImage wrapped_sub = world::build_hd_surface(map, &project, QRect(-2, 3, 4, 4), 1);
    ASSERT(wrapped_sub.size() == QSize(4, 4));
    ASSERT(wrapped_sub.pixel(0, 0) == coarse.pixel(30, 3));
    ASSERT(wrapped_sub.pixel(2, 0) == coarse.pixel(0, 3));

    // CloseRegionKey hits while the visible window stays inside the padded bounds.
    const QRectF zoomed_bounds(-320.0, -128.0, 2048.0, 1024.0);
    const QSize viewport(384, 256);
    const QRect visible = world::closeRegionTileBounds(zoomed_bounds, viewport, 32, 16, true, 0);
    const QRect padded = world::closeRegionTileBounds(zoomed_bounds, viewport, 32, 16, true, 4);
    const world::CloseRegionKey key{"lod_world", 7, padded, 64};
    ASSERT(key.contains("lod_world", 7, visible, 64));
    ASSERT(!key.contains("lod_world", 8, visible, 64));
    ASSERT(!key.contains("lod_world", 7, visible, 128));
    ASSERT(!key.contains("lod_world", 7, visible.translated(6, 0), 64));

    const QRect globe_vis = world::globeCloseRegionTileBounds(-0.6, -0.3, 64.0, viewport, 256, 128, 0);
    const QRect globe_pad = world::globeCloseRegionTileBounds(-0.6, -0.3, 64.0, viewport, 256, 128, 4);
    const world::CloseRegionKey globe_key{"world_globe", 3, globe_pad, 64};
    ASSERT(globe_key.contains("world_globe", 3, globe_vis, 64));
}
