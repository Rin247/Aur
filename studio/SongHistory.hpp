#pragma once

#include "CommandStack.hpp"
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"

#include <string>
#include <vector>

namespace aur::editor
{

    // One track-edit command over a v1/v2 song JSON: an undoable note change.
    // Commands store the affected track index and before/after note arrays for
    // that track (JSON copy — cheap at music-file sizes, keeps the model simple).
    struct SongAction
    {
        enum class Kind
        {
            NoteEdit,
            TrackRename,
            TrackProps
        };

        Kind kind = Kind::NoteEdit;
        int track_index = -1;
        std::string label; // human-readable, for menus/status
        Json before;       // full notes array (or scalar holder) pre-edit
        Json after;        // post-edit state
    };

    // Undoable song-edit facade: callers mutate, then commit the changed state;
    // undo restores the prior snapshot. Reuses the shared CommandStack.
    class SongHistory
    {
    public:
        explicit SongHistory(std::size_t limit = 100) : stack_(limit) {}

        // Record a completed note edit on a track. Call begin_note_edit()
        // before mutating the notes array, then commit_note_edit() after.
        void begin_note_edit(Json &song, int trackIndex);
        void commit_note_edit(Json &song, int trackIndex, std::string label);

        // Generic snapshot commit for track-level property edits.
        void commit_track_change(Json &song, int trackIndex, SongAction::Kind kind,
                                 std::string label);

        void undo(Json &song);
        void redo(Json &song);

        bool can_undo() const noexcept { return stack_.can_undo(); }
        bool can_redo() const noexcept { return stack_.can_redo(); }
        void clear() { stack_.clear(); }

    private:
        static Json &track_notes(Json &song, int trackIndex);
        static void apply(Json &song, const SongAction &action);
        static Json current_state(Json &song, const SongAction &action);
        static void apply_payload(Json &song, const SongAction &action,
                                  const Json &payload);

        CommandStack<SongAction> stack_;
        Json pendingBefore; // snapshot taken by begin_note_edit()
    };

} // namespace aur::editor
