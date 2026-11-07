#pragma once


#include "aur/json.hpp"

#include <string>
#include <vector>

namespace aur::audio {

// Song v2 model over the project JSON (contract 1: v1 keys stay readable;
// v2 adds keys and migrates on first save with a .bak backup).
//
// v2 layout (all additive):
//   format: "2"
//   ticks_per_step: 120          (480 PPQ / 4 steps per beat)
//   steps_per_beat: 4            (v1 assumed 4; now explicit)
//   tempo_map: [{step, bpm}]     (sorted; v1 "tempo" becomes entry at 0)
//   time_sig_map: [{step, num, den}] (v1 "beats" becomes entry at 0)
//   loop: {start_step, end_step} (v1 "loop_bar"Â·16; end = barsÂ·16 when unset)
//   markers: [{step, name}]
//   ... all v1 keys unchanged: name, bars, beats, gs, reverb, preview_synth,
//       tracks[{...notes[[step,pitch,length,velocity]]}], scenes[...]
class SongDocument {
public:
    static constexpr int kTicksPerStep = 120; // 480 PPQ / 4 steps per beat

    // Loads a song and migrates it in place to v2 (adds v2 keys, keeps v1).
    // Returns false if the JSON is not a song-shaped object.
    static bool load(Json& song);

    // Saves the song JSON to path; writes path + ".bak" from the previous
    // file first. Returns false on write failure.
    static bool save(const Json& song, const std::string& path);

    // Migration only (no I/O): fills v2 keys on a v1-shaped song. Idempotent.
    static void migrate(Json& song);

    // Accessors over the v2 keys with v1 fallbacks.
    static int bpm_at(const Json& song, int step);
    static int steps_per_beat(const Json& song);
    static int beats_at(const Json& song, int step);
    static int loop_start(const Json& song);
    static int loop_end(const Json& song);
    static void set_tempo_at(Json& song, int step, int bpm);
    static void set_time_sig_at(Json& song, int step, int num, int den);

private:
    struct MapPoint { int step; int value_a; int value_b; };

    static std::vector<MapPoint> read_map(const Json& song, const char* key);
    static void write_map(Json& song, const char* key,
                          const std::vector<MapPoint>& points, bool has_b);
    static int lookup(const std::vector<MapPoint>& points, int step,
                      int fallback);
};  // SongDocument
} // namespace aur::audio


