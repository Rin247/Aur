#pragma once

namespace aur::world
{

    // A point on the map's equirectangular grid: `u` wraps across longitude,
    // `v` runs 0 at the north pole to 1 at the south. Invisible off the disc.
    struct GlobePoint
    {
        bool visible = false;
        double u = 0.5;
        double v = 0.5;
    };

    // Orthographic disc point (nx across, ny up) to map coordinates; yaw/pitch spin it.
    GlobePoint projectToGlobe(double nx, double ny, double yaw, double pitch);

    // The shading factor at a disc point, from the sphere's normal: 1 facing
    // the viewer, dimmer toward the limb.
    double globeShade(double nx, double ny);

} // namespace aur::world
