#pragma once
#include "WorldModel.hpp"

#include <QPoint>
#include <set>
#include <vector>

namespace aur::world {

struct WorldStrokeState {
    bool active = false;
    QPoint last_tile{-1, -1};
    std::set<int> captured_cells;
    std::vector<editor::WorldModel::RectSnapshot> model_rects;

    void begin(const QPoint& tile);
    void end();
};

std::vector<QPoint> stroke_line_tiles(QPoint from, QPoint to, int wrap_width);

} // namespace aur::world
