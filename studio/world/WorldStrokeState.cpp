#include "world/WorldStrokeState.hpp"
#include "aur/world_topology.hpp"

#include <cmath>

namespace aur::world {

void WorldStrokeState::begin(const QPoint& tile) {
    active = true;
    last_tile = tile;
}

void WorldStrokeState::end() {
    active = false;
    last_tile = {-1, -1};
    captured_cells.clear();
    model_rects.clear();
}

std::vector<QPoint> stroke_line_tiles(QPoint from, QPoint to, int wrap_width) {
    std::vector<QPoint> tiles;
    if (to == from) return tiles;
    int delta_x = to.x() - from.x();
    if (wrap_width > 0) {
        delta_x = aur::wrapped_delta_x(aur::wrapX(from.x(), wrap_width),
                                       aur::wrapX(to.x(), wrap_width), wrap_width);
        to.setX(aur::wrapX(to.x(), wrap_width));
        from.setX(aur::wrapX(from.x(), wrap_width));
    }
    const int steps = std::max(std::abs(delta_x), std::abs(to.y() - from.y()));
    tiles.reserve(static_cast<std::size_t>(std::max(steps, 0)));
    for (int step = 1; step <= steps; ++step) {
        const double fraction = static_cast<double>(step) / steps;
        tiles.emplace_back(qRound(from.x() + delta_x * fraction),
                           qRound(from.y() + (to.y() - from.y()) * fraction));
    }
    return tiles;
}

} // namespace aur::world
