#include "world/WorldCommandHistory.hpp"

namespace aur::editor {

void WorldCommandHistory::record(Action&& action) {
    stack_.push_undo(std::move(action));
}

void WorldCommandHistory::record_entity(Json entities_state) {
    Action action;
    action.entities = true;
    action.state = std::move(entities_state);
    record(std::move(action));
}

void WorldCommandHistory::record_layers(Json layers_state) {
    Action action;
    action.entities = false;
    action.state = std::move(layers_state);
    record(std::move(action));
}

void WorldCommandHistory::record_rects(std::vector<WorldModel::RectSnapshot>&& rects) {
    if (rects.empty()) return;
    Action action;
    action.model_rects = std::move(rects);
    record(std::move(action));
}

void WorldCommandHistory::clear() {
    stack_.clear();
}

} // namespace aur::editor
