#include "mini_test.hpp"
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"

namespace
{

    aur::Json base_song()
    {
        return aur::parse_json(
            "{\"name\":\"Clips\",\"tempo\":120,\"bars\":2,\"beats\":4,"
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":40,"
            "\"notes\":[[0,60,4,100]]}],"
            "\"scenes\":[{\"name\":\"Scene 1\",\"clips\":[{\"notes\":[[0,60,4,100]]}]}]}");
    }

} // namespace

// P6.1: first-class clips — additive fields over the v1 {notes} shape.
void test_song_clips()
{
    using aur::audio::SongDocument;
    aur::Json song = base_song();

    // v1 bare {notes} clip reads with defaults.
    const aur::Json *clip = SongDocument::clip_at(song, 0, 0);
    ASSERT(clip != nullptr);
    const SongDocument::ClipInfo info = SongDocument::clip_info(*clip, "Fallback");
    ASSERT(info.name == "Fallback");
    ASSERT(info.start == 0 && info.length == 16 && !info.muted && info.color == -1);

    // Out-of-range lookups are null, never crash.
    ASSERT(SongDocument::clip_at(song, 5, 0) == nullptr);
    ASSERT(SongDocument::clip_at(song, 0, 9) == nullptr);
    ASSERT(SongDocument::clip_at_mut(song, -1, 0) == nullptr);

    // Set full info; keys stay additive and reload identically.
    SongDocument::ClipInfo full{"Lead · 1", 16, 32, true, 3};
    SongDocument::set_clip_info(*SongDocument::clip_at_mut(song, 0, 0), full);
    const SongDocument::ClipInfo reloaded =
        SongDocument::clip_info(*SongDocument::clip_at(song, 0, 0), "Fallback");
    ASSERT(reloaded.name == "Lead · 1");
    ASSERT(reloaded.start == 16 && reloaded.length == 32);
    ASSERT(reloaded.muted && reloaded.color == 3);

    // Length clamps into the legal range; start clamps at zero.
    SongDocument::ClipInfo weird{"X", -5, 99999, false, -1};
    SongDocument::set_clip_info(*SongDocument::clip_at_mut(song, 0, 0), weird);
    const SongDocument::ClipInfo clamped =
        SongDocument::clip_info(*SongDocument::clip_at(song, 0, 0), "Fallback");
    ASSERT(clamped.start == 0 && clamped.length == 2048);

    // The notes payload survives info writes untouched.
    ASSERT(SongDocument::clip_at(song, 0, 0)->contains("notes"));
    ASSERT(SongDocument::clip_at(song, 0, 0)->operator[]("notes").as_array().size() == 1);
}
