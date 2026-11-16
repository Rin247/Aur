#pragma once
#include "aur/project.hpp"

#include <QImage>
#include <QRect>

namespace aur::world {

// pixels_per_tile 0 is the surface's own density; a lower value is the
// same terrain at a coarser resolution, for a far view.
QImage build_hd_surface(const Json& map, const Project* project, const QRect& region,
                        int pixels_per_tile = 0);

}
