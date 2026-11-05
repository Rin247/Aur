#pragma once

#include "json.hpp"
#include <cstdint>
#include <vector>

namespace aur {

enum class PreviewSynth { Gm, Fm, RetroPsg };

struct StereoPcm {
    int sample_rate = 44100;
    std::vector<int16_t> samples;
};

StereoPcm synthesize_preview(const Json& song, PreviewSynth synth,
                             int sample_rate = 44100, int max_seconds = 180);

} // namespace aur
