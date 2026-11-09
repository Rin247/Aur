#include "mini_test.hpp"
#include "world/WorldCommandHistory.hpp"
#include "world/WorldStrokeState.hpp"
#include "world/WorldCamera.hpp"

#include <QPointF>
#include <QRectF>
#include <QSizeF>

void test_command_history_roundtrip()
{
    using aur::editor::WorldCommandHistory;
    WorldCommandHistory history;

    ASSERT(!history.can_undo());
    ASSERT(!history.can_redo());

    history.record_entity(aur::Json{1});
    history.record_entity(aur::Json{2});
    ASSERT(history.can_undo());
    ASSERT(!history.can_redo());

    auto action = history.take_undo();
    ASSERT(action.entities);
    ASSERT(action.state.as_int() == 2);
    ASSERT(history.can_undo());

    auto first = history.take_undo();
    ASSERT(first.state.as_int() == 1);
    ASSERT(!history.can_undo());

    history.push_redo(std::move(first));
    ASSERT(history.can_redo());
    auto redone = history.take_redo();
    ASSERT(redone.state.as_int() == 1);

    history.record_layers(aur::Json{"layers"});
    ASSERT(history.can_undo());
    ASSERT(!history.can_redo());
    auto layers = history.take_undo();
    ASSERT(!layers.entities);
    ASSERT(layers.state.as_string() == "layers");

    std::vector<aur::editor::WorldModel::RectSnapshot> rects(2);
    history.record_rects(std::move(rects));
    auto rect_action = history.take_undo();
    ASSERT(rect_action.model_rects.size() == 2);

    std::vector<aur::editor::WorldModel::RectSnapshot> none;
    history.record_rects(std::move(none));
    ASSERT(!history.can_undo());

    history.clear();
    ASSERT(!history.can_undo());
    ASSERT(!history.can_redo());
}

void test_command_history_limit()
{
    using aur::editor::WorldCommandHistory;
    WorldCommandHistory history;
    for (int i = 0; i < 105; ++i)
    {
        history.record_entity(aur::Json{i});
    }
    ASSERT(history.can_undo());
    auto action = history.take_undo();
    ASSERT(action.state.as_int() == 104);
    int depth = 1;
    while (history.can_undo())
    {
        static_cast<void>(history.take_undo());
        ++depth;
    }
    ASSERT(depth == 100);
}

void test_stroke_line_tiles()
{
    using aur::world::stroke_line_tiles;

    const std::vector<QPoint> empty = stroke_line_tiles({3, 4}, {3, 4}, 0);
    ASSERT(empty.empty());

    const std::vector<QPoint> plain = stroke_line_tiles({0, 0}, {3, 3}, 0);
    ASSERT(plain.size() == 3);
    ASSERT(plain.front() == QPoint(1, 1));
    ASSERT(plain.back() == QPoint(3, 3));

    const std::vector<QPoint> wrapped = stroke_line_tiles(QPoint(9, 0), QPoint(1, 0), 10);
    ASSERT(wrapped.size() == 2);
    ASSERT(wrapped.front() == QPoint(10, 0));
    ASSERT(wrapped.back() == QPoint(11, 0));

    const std::vector<QPoint> vertical = stroke_line_tiles({5, 2}, {5, 5}, 0);
    ASSERT(vertical.size() == 3);
    ASSERT(vertical.back() == QPoint(5, 5));
}

// WP1: the editor camera clamps to a single copy of a wrapping world unless
// repeat_view_x is set; data wrapping (stroke_line_tiles) is untouched.
void test_camera_repeat_view()
{
    using aur::studio::WorldCamera;
    using aur::studio::WorldCameraSpec;
    using aur::studio::WorldCameraState;

    WorldCameraSpec spec;
    spec.map_width = 10;
    spec.map_height = 10;
    spec.source_tile_size = 10.0;  // 100x100 world
    spec.wraps_x = true;
    spec.viewport = QSizeF(200.0, 200.0);  // larger than the map

    // Smaller-than-viewport map stays centred with repeat off.
    {
        WorldCameraState state;
        state.pan = QPointF(500.0, -500.0);
        WorldCamera::constrain(state, spec);
        ASSERT(state.pan.x() == 0.0);
        ASSERT(state.pan.y() == 0.0);
    }
    // Pan stops at the horizontal edges (24 px margin) on a large world.
    spec.map_width = 100;  // 1000x1000 world
    {
        WorldCameraState state;
        state.pan = QPointF(4000.0, 0.0);
        WorldCamera::constrain(state, spec);
        const qreal limit = (1000.0 - 200.0) * 0.5 + 24.0;
        ASSERT(state.pan.x() == limit);
        // Repeat on keeps the legacy unbounded wrap.
        spec.repeat_view_x = true;
        WorldCamera::constrain(state, spec);
        ASSERT(std::abs(state.pan.x()) <= 1000.0);
        spec.repeat_view_x = false;
    }
    // tileAt outside the single copy misses with repeat off...
    {
        WorldCameraSpec view_spec = spec;
        view_spec.pan = QPointF(0.0, 0.0);
        const QRectF bounds = WorldCamera::mapBounds(view_spec, view_spec.viewport);
        const QPoint outside(bounds.left() - 30.0, bounds.top() + 50.0);
        ASSERT(WorldCamera::tileAt(view_spec, outside) == QPoint(-1, -1));
        // ...and wraps with repeat on.
        view_spec.repeat_view_x = true;
        const QPoint wrapped = WorldCamera::tileAt(view_spec, outside);
        ASSERT(wrapped.y() == 5);
        ASSERT(wrapped.x() >= 0 && wrapped.x() < 100);
    }
}
