#include "camera.hpp"

#include <algorithm>
#include <cmath>

namespace aur
{

    Camera fieldCamera(const Body &body, const MoveWorld &world, double view_width,
                       double view_height)
    {
        const double map_width = world.width * world.tile_size;
        const double map_height = world.height * world.tile_size;
        Camera camera;
        camera.x = world.wraps_x ? body.x - view_width / 2.0
                                  : (map_width > view_width)
                                        ? std::clamp(body.x - view_width / 2.0, 0.0,
                                                     map_width - view_width)
                                        : (map_width - view_width) / 2.0;
        camera.y = (map_height > view_height)
                       ? std::clamp(body.y - 24.0 - view_height / 2.0, 0.0,
                                    map_height - view_height)
                       : (map_height - view_height) / 2.0;
        return camera;
    }

    Camera platformerCamera(const Body &body, const MoveWorld &world, double view_width,
                            double view_height, double dead_zone, const Camera &current)
    {
        const double map_width = world.width * world.tile_size;
        const double map_height = world.height * world.tile_size;
        Camera camera = current;
        camera.x = (map_width > view_width)
                       ? std::clamp(body.x - view_width / 2.0, 0.0, map_width - view_width)
                       : (map_width - view_width) / 2.0;

        // The dead zone: the camera holds its place until the hero leaves
        // the band around the middle of the view.
        const double centre = current.y + view_height / 2.0;
        const double half = dead_zone / 2.0;
        if (body.y < centre - half || body.y > centre + half)
        {
            camera.y = (map_height > view_height)
                           ? std::clamp(body.y - view_height / 2.0, 0.0, map_height - view_height)
                           : (map_height - view_height) / 2.0;
        }
        return camera;
    }

} // namespace aur
