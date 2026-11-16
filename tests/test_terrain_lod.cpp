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
}
