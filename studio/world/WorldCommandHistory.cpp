#include "world/WorldCommandHistory.hpp"

namespace aur::editor {

void WorldCommandHistory::record(Action&& action) {
    undo_.push_back(std::move(action));
    if (undo_.size() > kLimit) undo_.pop_front();
    redo_.clear();
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
    undo_.clear();
    redo_.clear();
}

WorldCommandHistory::Action WorldCommandHistory::take_undo() {
    Action action = std::move(undo_.back());
    undo_.pop_back();
    return action;
}

WorldCommandHistory::Action WorldCommandHistory::take_redo() {
    Action action = std::move(redo_.back());
    redo_.pop_back();
    return action;
}

void WorldCommandHistory::push_undo(Action action) {
    undo_.push_back(std::move(action));
    if (undo_.size() > kLimit) undo_.pop_front();
}

void WorldCommandHistory::push_redo(Action action) {
    redo_.push_back(std::move(action));
}

} // namespace aur::editor
