#include "mini_test.hpp"
#include "world/GlobeProjection.hpp"

#include <cmath>

void test_globe_projection() {
    // The centre of the disc faces the middle of the map.
    const aur::world::GlobePoint centre = aur::world::projectToGlobe(0.0, 0.0, 0.0, 0.0);
    ASSERT(centre.visible);
    ASSERT(std::abs(centre.u - 0.5) < 1e-9);
    ASSERT(std::abs(centre.v - 0.5) < 1e-9);

    // East is a longitude to the right of centre, north is a latitude above it.
    const aur::world::GlobePoint east = aur::world::projectToGlobe(0.5, 0.0, 0.0, 0.0);
    ASSERT(east.visible && east.u > 0.5);
    const aur::world::GlobePoint north = aur::world::projectToGlobe(0.0, 0.5, 0.0, 0.0);
    ASSERT(north.visible && north.v < 0.5);

    // The limb is the horizon and the poles sample the top and bottom rows.
    const aur::world::GlobePoint pole = aur::world::projectToGlobe(0.0, 1.0, 0.0, 0.0);
    ASSERT(pole.visible);
    ASSERT(pole.v < 1e-6);
    const aur::world::GlobePoint south = aur::world::projectToGlobe(0.0, -1.0, 0.0, 0.0);
    ASSERT(south.visible);
    ASSERT(south.v > 1.0 - 1e-6);

    // Outside the disc is not the sphere.
    ASSERT(!aur::world::projectToGlobe(0.9, 0.9, 0.0, 0.0).visible);
    ASSERT(!aur::world::projectToGlobe(1.5, 0.0, 0.0, 0.0).visible);

    // Yaw spins the same point to a different longitude; pitch raises it.
    const aur::world::GlobePoint turned = aur::world::projectToGlobe(0.0, 0.0, 1.0, 0.0);
    ASSERT(turned.visible && std::abs(turned.u - 0.5) > 0.1);
    const aur::world::GlobePoint tilted = aur::world::projectToGlobe(0.0, 0.0, 0.0, 0.6);
    ASSERT(tilted.visible && std::abs(tilted.v - 0.5) > 0.05);

    // Every sample is inside the map, and the longitude wraps.
    for (double ny = -1.0; ny <= 1.0; ny += 0.1) {
        for (double nx = -1.0; nx <= 1.0; nx += 0.1) {
            const aur::world::GlobePoint point = aur::world::projectToGlobe(nx, ny, 2.4, -0.7);
            if (!point.visible) continue;
            ASSERT(point.u >= 0.0 && point.u < 1.0);
            ASSERT(point.v >= 0.0 && point.v <= 1.0);
            ASSERT(!std::isnan(point.u) && !std::isnan(point.v));
        }
    }

    // The shading is brightest at the centre and dark toward the limb.
    ASSERT(aur::world::globeShade(0.0, 0.0) > aur::world::globeShade(0.9, 0.0));
    ASSERT(aur::world::globeShade(0.0, 0.0) <= 1.0);
}
