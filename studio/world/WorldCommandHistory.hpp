#pragma once
#include "WorldModel.hpp"
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

    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }
    Action take_undo();
    Action take_redo();
    void push_undo(Action action);
    void push_redo(Action action);

private:
    static constexpr std::size_t kLimit = 100;

    void record(Action&& action);

    std::deque<Action> undo_;
    std::deque<Action> redo_;
};

} // namespace aur::editor
