// Pitch regression test: every GM patch must render A4 at 440 Hz.
//
// Bug (2026-11-07..08): the sawtooth term in the voice DSP mixed radians
// (voice.phase[0], 0..2pi) with cycles (0..1), so families 5/6 swept the ramp
// twice per cycle and sounded an octave (or a fifth) high on organ/pad/lead
// patches — the "MIDI imports sound wrong" report. Zero-crossing measurement
// cannot catch this reliably (rich timbres double the count), so pitch is
// estimated by normalised autocorrelation, preferring the shortest lag.
#include "mini_test.hpp"
#include "aur/audio/synth.hpp"
#include "aur/json.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace
{

    double pitch_hz(const std::vector<std::int16_t> &samples, int rate)
    {
        const std::size_t start = 3000;
        const int lag_max = rate / 100;
        const int lag_min = rate / 2000;
        const std::size_t frames = samples.size() / 2;
        std::vector<double> correlation(static_cast<std::size_t>(lag_max) + 1, 0.0);
        double best = 0.0;
        for (int lag = lag_min; lag <= lag_max; ++lag)
        {
            double acc = 0.0, energy_a = 0.0, energy_b = 0.0;
            for (std::size_t i = start; i + static_cast<std::size_t>(lag) < frames && i < start + 8000; ++i)
            {
                const double a = samples[i * 2];
                const double b = samples[(i + static_cast<std::size_t>(lag)) * 2];
                acc += a * b;
                energy_a += a * a;
                energy_b += b * b;
            }
            correlation[static_cast<std::size_t>(lag)] =
                (energy_a > 0.0 && energy_b > 0.0) ? acc / std::sqrt(energy_a * energy_b) : 0.0;
            best = std::max(best, correlation[static_cast<std::size_t>(lag)]);
        }
        for (int lag = lag_min; lag < lag_max; ++lag)
        {
            const double here = correlation[static_cast<std::size_t>(lag)];
            if (here >= 0.9 * best &&
                here > correlation[static_cast<std::size_t>(lag - 1)] &&
                here >= correlation[static_cast<std::size_t>(lag + 1)])
                return static_cast<double>(rate) / lag;
        }
        return 0.0;
    }

    aur::StereoPcm render_patch(int program, int pitch)
    {
        const std::string json =
            "{\"name\":\"Pitch\",\"tempo\":120,\"bars\":4,\"beats\":4,\"tracks\":["
            "{\"name\":\"Lead\",\"channel\":0,\"program\":" + std::to_string(program) +
            ",\"volume\":100,\"pan\":64,\"notes\":[[0," + std::to_string(pitch) + ",64,100]]}]}";
        return aur::synthesize_preview(aur::parse_json(json), aur::PreviewSynth::Gm, 22050, 10);
    }

} // namespace

// Every GM program renders its fundamental at the requested note.
void test_synth_pitch_all_programs()
{
    int failures = 0;
    for (int program = 0; program < 128; ++program)
    {
        const double hz = pitch_hz(render_patch(program, 69).samples, 22050);
        if (!(hz > 425.0 && hz < 455.0))
        {
            std::printf("  program %d sounded %.1f Hz (want 440)\n", program, hz);
            ++failures;
        }
    }
    ASSERT(failures == 0);
    std::puts("  ok: all 128 GM programs sound A4 at 440 Hz");
}

// Pitch tracks the requested note, so octaves and fifths are not mis-scaled.
void test_synth_pitch_follows_note()
{
    const struct { int pitch; double hz; } cases[] = {
        {57, 220.0}, {69, 440.0}, {81, 880.0}, {60, 261.63}, {72, 523.25}};
    for (const auto &c : cases)
    {
        for (int program : {0, 19, 40, 48, 56, 80, 96, 112})
        {
            const double hz = pitch_hz(render_patch(program, c.pitch).samples, 22050);
            const double ratio = hz / c.hz;
            ASSERT(ratio > 0.96 && ratio < 1.04);
        }
    }
    std::puts("  ok: pitch tracks the requested note across octaves");
}
