#include "mini_test.hpp"
#include "aur/audio/midi.hpp"
#include "aur/audio/synth.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace {

aur::Json song_with_note_at(int step) {
    return aur::parse_json(
        "{\"name\":\"Clamp probe\",\"tempo\":120,\"bars\":128,\"beats\":4,"
        "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":0,"
        "\"notes\":[[" + std::to_string(step) + ",64,2,100]]}]}");
}

} // namespace

// The preview synth and MIDI export both clamped note steps to 546 (65535/120),
// silently truncating songs past ~bar 35 even though the UI allows 128 bars.
// Fixed in the DAW P0 commit; this test pins the fix.
void test_song_clamp_regression() {
    constexpr int kBeyondClamp = 600; // bar 38 at 16 steps/bar, past the old clamp

    const aur::Json song = song_with_note_at(kBeyondClamp);

    const aur::StereoPcm pcm = aur::synthesize_preview(song, aur::PreviewSynth::Gm, 8000, 600);
    // The buffer must span at least ~19 bars past the old clamp point so the
    // note has room to land, regardless of the engine's step-to-seconds ratio.
    ASSERT(pcm.samples.size() >= static_cast<std::size_t>(600 * 8000 / 8));

    // Single-note song: silence up front, the note rendered somewhere later.
    // Locate the first non-zero frame instead of trusting a tempo mapping.
    std::size_t first_sound = pcm.samples.size();
    std::size_t last_sound = 0;
    double late_energy = 0.0;
    for (std::size_t i = 0; i < pcm.samples.size(); i += 2) {
        const int s = pcm.samples[i];
        if (s == 0) continue;
        if (i < first_sound) first_sound = i;
        last_sound = i;
        if (i > pcm.samples.size() / 2) late_energy += double(s) * s;
    }
    ASSERT(first_sound < last_sound);
    ASSERT(first_sound > pcm.samples.size() / 4); // not rendered at step 0
    ASSERT(last_sound > first_sound);             // the note spans real audio

    const std::vector<std::uint8_t> bytes = aur::export_standard_midi(song);
    const aur::Json imported = aur::import_standard_midi(bytes, "Round trip");
    ASSERT(imported["tracks"].as_array().size() == 1);
    ASSERT(imported["tracks"][0]["notes"].as_array().size() == 1);
    ASSERT(imported["tracks"][0]["notes"][0][0].as_int() == kBeyondClamp);
}
