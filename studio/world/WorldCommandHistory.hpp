#pragma once
#include "WorldModel.hpp"
#include "CommandStack.hpp"
#include "aur/json.hpp"

#include <cstddef>
#include <deque>
#include <vector>

namespace aur::editor {

class WorldCommandHistory {
public:
    struct Action {
        bool entities = false;
        Json state;
        std::vector<WorldModel::RectSnapshot> model_rects;
    };

    void record_entity(Json entities_state);
    void record_layers(Json layers_state);
    void record_rects(std::vector<WorldModel::RectSnapshot>&& rects);
    void clear();

    bool can_undo() const noexcept { return stack_.can_undo(); }
    bool can_redo() const noexcept { return stack_.can_redo(); }
    Action take_undo() { return stack_.take_undo(); }
    Action take_redo() { return stack_.take_redo(); }
    void push_undo(Action action) { stack_.push_undo(std::move(action)); }
    void push_redo(Action action) { stack_.push_redo(std::move(action)); }

private:
    static constexpr std::size_t kLimit = 100;

    void record(Action&& action);

    CommandStack<Action> stack_{kLimit};
};

} // namespace aur::editor
