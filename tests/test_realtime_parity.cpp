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
            "\"preview_synth\":\"md_fm\","
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":40,"
            "\"volume\":100,\"pan\":64,"
            "\"notes\":[[0,64,4,100],[4,67,4,96],[8,72,4,100],[12,64,2,80]]},"
            "{\"name\":\"Drums\",\"channel\":9,\"program\":0,\"volume\":90,\"pan\":64,"
            "\"notes\":[[0,36,1,100],[4,38,1,90],[8,36,1,100],[12,42,1,70]]}]}");
    }

} // namespace

// Core-synth parity (md_fm / spc): the live mixer and the offline preview both
// render through one SynthCoreVoice, so they must agree on the first loop pass.
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

// The master volume: the GUI thread sets it, the audio thread reads it, and
// the mix is scaled by it. A song's own volume is not a second number.
void test_master_volume()
{
    constexpr int rate = 8000;
    constexpr int frames = 800;

    aur::audio::RealtimeMixer mixer;
    mixer.setSampleRate(rate);
    mixer.loadSong(parity_song());
    std::vector<float> full(static_cast<size_t>(frames) * 2, 0.0f);
    mixer.render(full.data(), frames);

    double full_energy = 0.0;
    for (float sample : full)
        full_energy += static_cast<double>(sample) * sample;
    ASSERT(full_energy > 0.0);

    // Half the volume is half the energy, and the mix is not silent.
    aur::audio::RealtimeMixer half;
    half.setSampleRate(rate);
    half.setMasterVolume(0.5f);
    half.loadSong(parity_song());
    std::vector<float> quiet(static_cast<size_t>(frames) * 2, 0.0f);
    half.render(quiet.data(), frames);

    double quiet_energy = 0.0;
    for (float sample : quiet)
        quiet_energy += static_cast<double>(sample) * sample;
    std::printf("  master volume full=%g half=%g\n", full_energy, quiet_energy);
    ASSERT(std::abs(quiet_energy / full_energy - 0.25) < 0.001);

    // Zero is silence.
    aur::audio::RealtimeMixer muted;
    muted.setSampleRate(rate);
    muted.setMasterVolume(0.0f);
    muted.loadSong(parity_song());
    std::vector<float> silence(static_cast<size_t>(frames) * 2, 0.0f);
    muted.render(silence.data(), frames);
    for (float sample : silence)
        ASSERT(sample == 0.0f);

    // The gain is clamped to 0..1, and it can move while the song plays.
    aur::audio::RealtimeMixer moving;
    moving.setSampleRate(rate);
    moving.loadSong(parity_song());
    moving.setMasterVolume(2.0f);
    ASSERT(moving.masterVolume() == 1.0f);
    moving.setMasterVolume(-1.0f);
    ASSERT(moving.masterVolume() == 0.0f);
}
