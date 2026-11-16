#pragma once

#include <cstddef>
#include <deque>
#include <utility>

namespace aur::editor {

// Shared undo/redo stack discipline: bounded history, redo cleared on new
// work, LIFO undo/redo. Domain layers own their Action payloads; this class
// owns only ordering and limits. Estate rule: built in the common layer
// first, reused by the world editor and the music editor.
template <typename Action>
class CommandStack {
public:
    explicit CommandStack(std::size_t limit = 100) : limit_(limit) {}

    void push_undo(Action action) {
        undo_.push_back(std::move(action));
        if (undo_.size() > limit_) undo_.pop_front();
        redo_.clear();
    }

    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }

    Action take_undo() {
        Action action = std::move(undo_.back());
        undo_.pop_back();
        return action;
    }

    Action take_redo() {
        Action action = std::move(redo_.back());
        redo_.pop_back();
        return action;
    }

    void push_redo(Action action) {
        redo_.push_back(std::move(action));
        if (redo_.size() > limit_) redo_.pop_front();
    }

    void clear() {
        undo_.clear();
        redo_.clear();
    }

    std::size_t undo_depth() const noexcept { return undo_.size(); }
    std::size_t redo_depth() const noexcept { return redo_.size(); }

private:
    std::size_t limit_;
    std::deque<Action> undo_;
    std::deque<Action> redo_;
};

} // namespace aur::editor
