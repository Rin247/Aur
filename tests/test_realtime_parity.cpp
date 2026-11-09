#include "mini_test.hpp"
#include "aur/audio/RealtimeMixer.hpp"
#include "aur/audio/synth.hpp"

#include <cmath>
#include <string>

namespace
{

    aur::Json parity_song()
    {
        return aur::parse_json(
            "{\"name\":\"Parity\",\"tempo\":120,\"bars\":1,\"beats\":4,"
            "\"preview_synth\":\"gm\","
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":40,"
            "\"volume\":100,\"pan\":64,"
            "\"notes\":[[0,64,4,100],[4,67,4,96],[8,72,4,100],[12,64,2,80]]},"
            "{\"name\":\"Drums\",\"channel\":9,\"program\":0,\"volume\":90,\"pan\":64,"
            "\"notes\":[[0,36,1,100],[4,38,1,90],[8,36,1,100],[12,42,1,70]]}]}");
    }

} // namespace

// Contract 5: the real-time mixer must render the same PCM as the offline
// preview for the same song and sample rate (float tolerance documented
// below). Both paths share VoiceCore DSP; this test pins that coupling.
void test_realtime_parity()
{
    const aur::Json song = parity_song();
    constexpr int rate = 8000;

    const aur::StereoPcm offline = aur::synthesize_preview(
        song, aur::PreviewSynth::Gm, rate, 30);

    // Compare the first loop pass only (16 steps = 16000 frames at 8 kHz): the
    // live mixer loops, the offline render does not, so past the wrap they
    // legitimately diverge. Parity contract covers one pass.
    const int frames = std::min(static_cast<int>(offline.samples.size() / 2),
                                16 * rate * 60 / (120 * 4));

    aur::audio::RealtimeMixer mixer;
    mixer.setSampleRate(rate);
    mixer.loadSong(song);
    std::vector<float> live(static_cast<size_t>(frames) * 2, 0.0f);
    mixer.render(live.data(), frames);

    ASSERT(offline.samples.size() >= static_cast<size_t>(frames) * 2);

    // Per-sample tolerance: the offline path quantises to int16, the live
    // path stays float; allow 1 LSB of quantisation plus a small float slack.
    double max_delta = 0.0;
    double sum_ref_sq = 0.0;
    for (int i = 0; i < frames * 2; ++i)
    {
        const double ref = offline.samples[static_cast<size_t>(i)] / 32768.0;
        const double got = live[static_cast<size_t>(i)];
        max_delta = std::max(max_delta, std::abs(ref - got));
        sum_ref_sq += ref * ref;
    }
    const double rel = max_delta / std::sqrt(sum_ref_sq + 1e-12);
    std::printf("  parity max_delta=%g rel=%g\n", max_delta, rel);
    ASSERT(rel < 0.0005);
}

// Core-synth parity (md_fm / spc): the live mixer and the offline preview
// both render through one SynthCoreVoice, so they must agree to the same
// tolerance on the first loop pass.
void test_realtime_parity_core_synths()
{
    constexpr int rate = 8000;
    for (const char *synthId : {"md_fm", "spc"})
    {
        aur::Json song = aur::parse_json(
            std::string("{\"name\":\"CoreParity\",\"tempo\":120,\"bars\":1,\"beats\":4,"
                        "\"preview_synth\":\"") +
            synthId +
            "\",\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":0,"
            "\"volume\":100,\"pan\":64,"
            "\"notes\":[[0,60,4,100],[4,64,4,96],[8,67,4,100],[12,72,2,80]]}]}");
        song["preview_synth"] = synthId;

        const aur::PreviewSynth synth = std::string(synthId) == "spc"
                                            ? aur::PreviewSynth::Spc
                                            : aur::PreviewSynth::MdFm;
        const aur::StereoPcm offline = aur::synthesize_preview(song, synth, rate, 30);
        const int frames = std::min(static_cast<int>(offline.samples.size() / 2),
                                    16 * rate * 60 / (120 * 4));

        aur::audio::RealtimeMixer mixer;
        mixer.setSampleRate(rate);
        mixer.loadSong(song);
        std::vector<float> live(static_cast<size_t>(frames) * 2, 0.0f);
        mixer.render(live.data(), frames);

        double max_delta = 0.0;
        double sum_ref_sq = 0.0;
        for (int i = 0; i < frames * 2; ++i)
        {
            const double ref = offline.samples[static_cast<size_t>(i)] / 32768.0;
            const double got = live[static_cast<size_t>(i)];
            max_delta = std::max(max_delta, std::abs(ref - got));
            sum_ref_sq += ref * ref;
        }
        const double rel = max_delta / std::sqrt(sum_ref_sq + 1e-12);
        std::printf("  parity %s max_delta=%g rel=%g\n", synthId, max_delta, rel);
        ASSERT(rel < 0.0005);
    }
}
