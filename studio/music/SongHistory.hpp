#pragma once

#include "core/CommandStack.hpp"
#include "aur/audio/SongDocument.hpp"

#include "aur/json.hpp"

#include <string>
#include <vector>

namespace aur::editor
{

    // One track-edit command over a v1/v2 song JSON. Commands store the affected
    // track index and before/after note arrays (JSON copy — cheap at music-file sizes).
    struct SongAction
    {
        enum class Kind
        {
            NoteEdit,
            TrackRename,
            TrackProps,
            Groovebox
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

        // Record a completed track-level edit (rename, props, per-note data).
        // begin_track_edit() snapshots the track; commit_track_change() pairs it.
        void begin_track_edit(Json &song, int trackIndex);
        void commit_track_change(Json &song, int trackIndex, SongAction::Kind kind,
                                 std::string label);

        // Record a groovebox edit (step patterns, swing/humanise/groove, the
        // arpeggiator and the chain). These live at song level, so they carry their own.
        void begin_groovebox_edit(Json &song);
        void commit_groovebox_change(Json &song, std::string label);

        void undo(Json &song);
        void redo(Json &song);

        bool can_undo() const noexcept { return stack_.can_undo(); }
        bool can_redo() const noexcept { return stack_.can_redo(); }
        void clear() { stack_.clear(); }

    private:
        static Json &track_notes(Json &song, int trackIndex);
        static Json groovebox_state(Json &song);
        static void apply(Json &song, const SongAction &action);
        static Json current_state(Json &song, const SongAction &action);
        static void apply_payload(Json &song, const SongAction &action,
                                  const Json &payload);

        CommandStack<SongAction> stack_;
        Json pendingBefore;      // notes snapshot taken by begin_note_edit()
        Json pendingTrackBefore; // track snapshot taken by begin_track_edit()
        Json pendingGrooveBefore; // groovebox snapshot taken by begin_groovebox_edit()
    };

} // namespace aur::editor
