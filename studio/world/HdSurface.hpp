#pragma once
#include "aur/project.hpp"

#include <QImage>
#include <QRect>

namespace aur::world {

QImage build_hd_surface(const Json& map, const Project* project, const QRect& region);

}
