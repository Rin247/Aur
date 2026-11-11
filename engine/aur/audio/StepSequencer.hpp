// StepSequencer.hpp - the 128-step pattern model and its compiler.
// Qt-free: the pattern compiles to an immutable event list that the mixer
// and the offline renderer both consume, so both paths stay identical.
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "aur/json.hpp"

namespace aur::audio
{

    // One cell of a step pattern. Steps are sparse in the song JSON: only
    // cells that carry data are stored.
    struct StepCell
    {
        int step = 0;      // cell index inside the pattern
        int pitch = 60;    // MIDI pitch
        int velocity = 90; // 0..127
        int accent = 0;    // 0 = plain, 1 = accented (velocity boost)
        int slide = 0;     // semitones to glide from (0 = no slide)
        int ratchet = 0;   // extra retriggers inside the cell
        bool tie = false;  // holds into the next cell
    };

    // A 128-step pattern bound to one song track and one MIDI channel.
    struct StepPattern
    {
        std::string name = "Pattern";
        int track = 0;        // song track index
        int channel = 0;      // MIDI channel (9 = drums)
        int program = 0;      // GM program
        int length = 128;     // cells in the pattern (1..128)
        int resolution = 4;   // song steps per cell (1, 2, 4, 8, 16)
        int seed = 1;         // deterministic seed for the randomisers
        std::vector<StepCell> cells;
    };

    // One compiled note event, in song-step coordinates. The step is a
    // fractional step so swing and humanise can offset a cell without
    // re-snapping to the 1/16 grid.
    struct StepEvent
    {
        double step = 0.0;    // fractional song step of the note-on
        int pitch = 60;
        int velocity = 90;
        double length = 2.0;  // length in song steps
        int track = 0;
        int channel = 0;
        int program = 0;
    };

    // Timing knobs shared by every pattern in the song. Swing delays the odd
    // cells of a bar; humanise jitters each cell by a seeded amount.
    struct GrooveSettings
    {
        int swing = 0;      // 0..100, percent of the second half delayed
        int humanize = 0;   // 0..100, timing jitter amount
        int groove = 0;     // -50..50, master velocity offset
    };

    // Reads song["step_patterns"] into pattern structs. A song without the
    // key reads as an empty list.
    std::vector<StepPattern> read_step_patterns(const Json &song);

    // Reads song["groovebox"] into the timing knobs. A song without the key
    // reads as neutral.
    GrooveSettings read_groove_settings(const Json &song);

    // Compiles one pattern into note events. Swing and humanise are applied
    // here, so every consumer gets the same timing.
    std::vector<StepEvent> compile_pattern(const StepPattern &pattern,
                                            const GrooveSettings &groove);

    // Compiles the chain (song["groovebox"]["chain"], a list of pattern
    // indices) into one event list, in chain order. An empty or invalid
    // chain compiles every pattern once, in song order.
    std::vector<StepEvent> compile_chain(const Json &song);

    // Deterministic per-cell jitter in [-1, 1] for a pattern seed and cell.
    double humanize_offset(int seed, int cell);

} // namespace aur::audio
