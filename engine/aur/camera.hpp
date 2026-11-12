#pragma once

// Camera.hpp - the engine's cameras.
//
// The field's camera is the hero's position, clamped to the map and
// wrap-aware in x, as it has always been. The platformer's scrolls
// sideways, with a vertical dead zone so it does not jitter, and a room
// does not wrap.

#include "movement.hpp"

namespace aur
{

    struct Camera
    {
        double x = 0;
        double y = 0;
    };

    // The field's camera: unchanged.
    Camera fieldCamera(const Body &body, const MoveWorld &world, double view_width,
                       double view_height);

    // The platformer's camera: sideways, with a dead zone in y, clamped
    // to the room. `current` is where the camera is now, so the dead
    // zone has something to hold.
    Camera platformerCamera(const Body &body, const MoveWorld &world, double view_width,
                            double view_height, double dead_zone, const Camera &current);

} // namespace aur
