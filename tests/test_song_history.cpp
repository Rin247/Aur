#include "mini_test.hpp"
#include "music/SongHistory.hpp"
#include "aur/json.hpp"

namespace
{

    aur::Json base_song()
    {
        return aur::parse_json(
            "{\"name\":\"Undo\",\"tempo\":120,\"bars\":4,\"beats\":4,"
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":40,"
            "\"notes\":[[0,60,4,100],[8,64,4,100]]}]}");
    }

    const aur::JsonArray &notes(const aur::Json &song)
    {
        return song["tracks"][0]["notes"].as_array();
    }

    aur::JsonArray &notes(aur::Json &song)
    {
        return song["tracks"][0]["notes"].as_array();
    }

} // namespace

// P3.5: per-note properties.
void test_note_props()
{
    using aur::Json;
    Json note = aur::JsonArray{4, 60, 8, 100};
    ASSERT(aur::audio::SongDocument::note_velocity(note) == 100);
    ASSERT(aur::audio::SongDocument::note_props(note) == nullptr);

    aur::audio::SongDocument::set_note_velocity(note, 200); // clamped
    ASSERT(aur::audio::SongDocument::note_velocity(note) == 127);

    aur::audio::SongDocument::set_note_prop(note, "articulation", 3);
    const aur::Json *props = aur::audio::SongDocument::note_props(note);
    ASSERT(props && props->contains("articulation") &&
           (*props)["articulation"].as_int() == 3);
    // note stays [step,pitch,length,velocity,{props}] — 5 slots, velocity intact
    ASSERT(note.as_array().size() == 5);
    ASSERT(aur::audio::SongDocument::note_velocity(note) == 127);

    aur::audio::SongDocument::clear_note_props(note);
    ASSERT(note.as_array().size() == 4);
    ASSERT(aur::audio::SongDocument::note_props(note) == nullptr);

    // v1-shaped short note falls back to the default velocity.
    Json shortNote = aur::JsonArray{0, 60, 4};
    ASSERT(aur::audio::SongDocument::note_velocity(shortNote) == 90);
}

// P3.3: typed song edits are undoable and redoable on the shared stack.
void test_song_history()
{
    aur::editor::SongHistory history;
    aur::Json song = base_song();
    const aur::Json original = song;

    // Add a note: snapshot, mutate, commit.
    history.begin_note_edit(song, 0);
    notes(song).push_back(aur::JsonArray{12, 67, 4, 90});
    history.commit_note_edit(song, 0, "add note");
    ASSERT(notes(song).size() == 3);
    ASSERT(history.can_undo());

    // Undo restores the two-note state.
    history.undo(song);
    ASSERT(notes(song).size() == 2);
    ASSERT(history.can_redo());
    ASSERT(!history.can_undo()); // only one action was committed

    // Redo restores the three-note state.
    history.redo(song);
    ASSERT(notes(song).size() == 3);
    ASSERT(!history.can_redo());

    // Undo/redo of a note edit does not disturb track props or song keys.
    history.undo(song);
    ASSERT(song["tracks"][0]["program"].as_int() == 40);
    ASSERT(song["tempo"].as_int() == 120);
    ASSERT(notes(song).size() == 2);

    // Track-property edit: begin_track_edit snapshots the whole track before
    // mutation; commit_track_change pairs it with the post-edit state.
    history.begin_track_edit(song, 0);
    song["tracks"][0]["volume"] = 64;
    history.commit_track_change(song, 0, aur::editor::SongAction::Kind::TrackProps,
                                "volume");
    ASSERT(song["tracks"][0]["volume"].as_int() == 64);
    ASSERT(history.can_undo());
    history.undo(song);
    ASSERT(!song["tracks"][0].contains("volume"));
    ASSERT(song["tracks"][0]["notes"].as_array().size() == 2);
    history.redo(song);
    ASSERT(song["tracks"][0]["volume"].as_int() == 64);

    // Depth: many edits, undo walks back through all of them.
    aur::editor::SongHistory deep;
    aur::Json deepSong = base_song();
    for (int i = 0; i < 150; ++i)
    {
        deep.begin_note_edit(deepSong, 0);
        notes(deepSong).push_back(aur::JsonArray{i, 60, 1, 80});
        deep.commit_note_edit(deepSong, 0, "bulk");
    }
    ASSERT(notes(deepSong).size() == 152);
    int steps = 0;
    while (deep.can_undo())
    {
        deep.undo(deepSong);
        ++steps;
    }
    ASSERT(steps == 100); // bounded history keeps the last 100
    ASSERT(notes(deepSong).size() == 52);
    ASSERT(deep.can_redo());
}
