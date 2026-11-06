#include "mini_test.hpp"
#include "world/WorldCommandHistory.hpp"

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
