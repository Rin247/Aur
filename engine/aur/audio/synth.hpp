#pragma once

#include "aur/json.hpp"
#include <atomic>

#include <cstdint>

#include <functional>

#include <vector>

namespace aur {

// The preview instruments. The three oscillator synths that came first (Gm, Fm,
// RetroPsg) were retired; their song ids now fall through to the YM-FM core.
enum class PreviewSynth : int { MdFm = 0, Spc = 1 };

struct StereoPcm {
    int sample_rate = 44100;
    std::vector<int16_t> samples;
};

// Optional cooperative cancel (checked between voice frames) and progress
// callback (fraction 0..1, called from the rendering thread).
using RenderProgress = std::function<void(double)>;
StereoPcm synthesize_preview(const Json& song, PreviewSynth synth,
                             int sample_rate = 44100, int max_seconds = 180,
                             const std::atomic<bool>* cancel = nullptr,
                             const RenderProgress& progress = {});

} // namespace aur
