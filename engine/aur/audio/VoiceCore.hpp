#pragma once

#include <cstdint>
#include <vector>
#include "aur/json.hpp"

namespace aur {
namespace audio {

// Shared note state for every voice renderer. Owned by whoever schedules
// notes; renderers only advance phase and produce samples.
struct Voice {
    double frequency = 0.0;
    double amplitude = 0.0;
    double pan = 0.5;
    double phase[6]{};
    int start = 0;      // frame index in the render timeline
    int end = 0;        // frame index after which the voice is silent
    int program = 0;
    int pitch = 0;
    bool drum = false;
};

enum class Timbre { Gm, Fm, RetroPsg };

// Advances the voice and returns one monophonic sample. Pure DSP, shared by
// the offline preview and the real-time mixer; identical inputs give
// identical samples (contract 5).
double render_voice_sample(Voice& voice, Timbre timbre,
                           double time, std::int64_t frame, int sample_rate);

// Attack/release envelope shared with render_voice_sample consumers.
double voice_envelope(const Voice& voice, std::int64_t frame, int sample_rate);

// Builds a Voice from a v1 track/note pair.
Voice voice_from_note(const Json& track, const Json& note, int bpm,
                      int sample_rate, std::int64_t startFrame);

} // namespace audio
} // namespace aur
