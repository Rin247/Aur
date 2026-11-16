#include "music/SongHistory.hpp"

#include <utility>

namespace aur::editor
{

    Json &SongHistory::track_notes(Json &song, int trackIndex)
    {
        Json &tracks = song["tracks"];
        Json &track = tracks[static_cast<size_t>(trackIndex)];
        if (!track.contains("notes") || !track["notes"].is_array())
        {
            track["notes"] = JsonArray{};
        }
        return track["notes"];
    }

    void SongHistory::commit_note_edit(Json &song, int trackIndex,
                                       std::string label)
    {
        // Callers mutate the notes array FIRST, then commit here: this snapshots
        // the current (post-edit) notes as `after`; `before` comes from begin_note_edit().
        SongAction action;
        action.kind = SongAction::Kind::NoteEdit;
        action.track_index = trackIndex;
        action.label = std::move(label);
        action.after = track_notes(song, trackIndex);
        action.before = pendingBefore;
        pendingBefore = Json{};
        stack_.push_undo(std::move(action));
    }

    void SongHistory::begin_note_edit(Json &song, int trackIndex)
    {
        pendingBefore = track_notes(song, trackIndex);
    }

    void SongHistory::begin_track_edit(Json &song, int trackIndex)
    {
        pendingTrackBefore = song["tracks"][static_cast<size_t>(trackIndex)];
    }

    void SongHistory::commit_track_change(Json &song, int trackIndex,
                                          SongAction::Kind kind,
                                          std::string label)
    {
        SongAction action;
        action.kind = kind;
        action.track_index = trackIndex;
        action.label = std::move(label);
        action.before = std::move(pendingTrackBefore);
        pendingTrackBefore = Json{};
        action.after = song["tracks"][static_cast<size_t>(trackIndex)];
        stack_.push_undo(std::move(action));
    }

    void SongHistory::begin_groovebox_edit(Json &song)
    {
        pendingGrooveBefore = groovebox_state(song);
    }

    void SongHistory::commit_groovebox_change(Json &song, std::string label)
    {
        SongAction action;
        action.kind = SongAction::Kind::Groovebox;
        action.track_index = -1;
        action.label = std::move(label);
        action.before = std::move(pendingGrooveBefore);
        pendingGrooveBefore = Json{};
        action.after = groovebox_state(song);
        stack_.push_undo(std::move(action));
    }

    Json SongHistory::groovebox_state(Json &song)
    {
        Json state;
        state["step_patterns"] = song.contains("step_patterns")
                                     ? song["step_patterns"]
                                     : Json(JsonArray{});
        state["groovebox"] = song.contains("groovebox") ? song["groovebox"] : Json(JsonObject{});
        return state;
    }

    Json SongHistory::current_state(Json &song, const SongAction &action)
    {
        if (action.kind == SongAction::Kind::NoteEdit)
            return Json(track_notes(song, action.track_index));
        if (action.kind == SongAction::Kind::Groovebox)
            return groovebox_state(song);
        return Json(song["tracks"][static_cast<size_t>(action.track_index)]);
    }

    void SongHistory::apply_payload(Json &song, const SongAction &action,
                                    const Json &payload)
    {
        SongAction staged = action;
        staged.after = payload;
        apply(song, staged);
    }

    void SongHistory::apply(Json &song, const SongAction &action)
    {
        if (action.kind == SongAction::Kind::Groovebox)
        {
            if (action.after.contains("step_patterns"))
                song["step_patterns"] = action.after["step_patterns"];
            if (action.after.contains("groovebox"))
                song["groovebox"] = action.after["groovebox"];
            return;
        }
        if (action.track_index < 0 ||
            !song.contains("tracks") || !song["tracks"].is_array() ||
            action.track_index >= static_cast<int>(song["tracks"].as_array().size()))
        {
            return;
        }
        Json &track = song["tracks"][static_cast<size_t>(action.track_index)];
        switch (action.kind)
        {
        case SongAction::Kind::NoteEdit:
            track["notes"] = action.after;
            break;
        case SongAction::Kind::TrackRename:
        case SongAction::Kind::TrackProps:
        default:
            track = action.after;
            break;
        }
    }

    void SongHistory::undo(Json &song)
    {
        if (!stack_.can_undo())
            return;
        SongAction action = stack_.take_undo();
        Json current = current_state(song, action);
        Json undoPayload = std::move(action.before);
        apply_payload(song, action, undoPayload);
        action.before = Json{};
        action.after = std::move(current);
        stack_.push_redo(std::move(action));
    }

    void SongHistory::redo(Json &song)
    {
        if (!stack_.can_redo())
            return;
        SongAction action = stack_.take_redo();
        Json current = current_state(song, action);
        Json redoPayload = std::move(action.after);
        apply_payload(song, action, redoPayload);
        action.after = Json{};
        action.before = std::move(current);
        stack_.push_undo(std::move(action));
    }

} // namespace aur::editor
