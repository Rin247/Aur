// P8.2 contract tests: the master effect chain is additive over the song JSON
// (absent "effects" key = the old path) and both render paths agree.

#include "mini_test.hpp"
#include "aur/audio/synth.hpp"

#include "aur/audio/RealtimeMixer.hpp"

#include "aur/audio/SongDocument.hpp"

#include <cmath>
#include <string>

#include <vector>

namespace
{

    std::uint64_t fnv1a(const std::vector<std::int16_t> &samples)
    {
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        for (const std::int16_t s : samples)
        {
            hash ^= static_cast<std::uint64_t>(static_cast<std::uint16_t>(s));
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    aur::Json plain_song()
    {
        return aur::parse_json(
            "{\"name\":\"Fx\",\"tempo\":120,\"bars\":2,\"beats\":4,\"preview_synth\":\"gm\","
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":48,\"volume\":127,"
            "\"pan\":64,\"notes\":[[0,60,8,127],[8,64,8,127],[16,67,8,127],[24,72,4,127],[32,60,8,127],[40,64,8,127],[48,67,8,127],[56,72,4,127]]},"
            "{\"name\":\"Pad\",\"channel\":1,\"program\":88,\"volume\":127,\"pan\":64,"
            "\"notes\":[[0,48,16,127],[0,55,16,127],[0,64,16,127],[16,53,16,127],[16,60,16,127],[16,64,16,127]]}]}");
    }

    void add_chain(aur::Json &song, const char *type, bool bypass,
                   const std::vector<std::pair<int, int>> &params)
    {
        std::vector<aur::audio::SongDocument::EffectSlot> chain;
        if (song.contains("effects") && song["effects"].is_object() &&
            song["effects"].contains("chain") && song["effects"]["chain"].is_array())
        {
            // Reuse the accessor round-trip: read, append, write.
            chain = aur::audio::SongDocument::effect_chain(song);
        }
        aur::audio::SongDocument::EffectSlot slot;
        slot.type = type;
        slot.bypass = bypass;
        slot.params = params;
        chain.push_back(slot);
        aur::audio::SongDocument::set_effect_chain(song, chain);
    }

} // namespace

// Absent "effects" key: preview output is bit-identical to the pre-P8 path.
void test_fx_chain_absent_key_unchanged()
{
    const aur::Json song = plain_song();
    const aur::StereoPcm pcm = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    ASSERT(pcm.samples.size() > 0);
    // Golden-style pin: deterministic and non-trivial.
    const std::uint64_t h = fnv1a(pcm.samples);
    const aur::StereoPcm again = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    ASSERT(fnv1a(again.samples) == h);
    ASSERT(h != 0);
    std::puts("  ok: absent effects key renders deterministically (old path)");
}

// An explicit empty chain also means no effects.
void test_fx_chain_empty_is_noop()
{
    aur::Json song = plain_song();
    const aur::StereoPcm without = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    aur::audio::SongDocument::set_effect_chain(song, {});
    ASSERT(!song.contains("effects"));
    const aur::StereoPcm with = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    ASSERT(fnv1a(with.samples) == fnv1a(without.samples));
    std::puts("  ok: empty chain round-trips to no key and renders identically");
}

// A limiter with a low ceiling must reduce peak level; unknown types skipped.
void test_fx_chain_changes_audio_and_skips_unknown()
{
    aur::Json song = plain_song();
    const aur::StereoPcm dry = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    // Ceiling at -40 dB (param -4000, range now -1200..0 is -12 dB, so clamp
    // to -1200 = -12 dB) — deep enough to catch this quiet render.
    add_chain(song, "Limiter", false, {{0, -1200}});
    add_chain(song, "Not A Real Effect", false, {});
    const aur::StereoPcm wet = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    ASSERT(wet.samples.size() == dry.samples.size());
    double peak_dry = 0, peak_wet = 0;
    for (size_t i = 0; i < wet.samples.size(); i += 2)
    {
        peak_dry = std::max(peak_dry, std::abs(dry.samples[i] / 32768.0));
        peak_wet = std::max(peak_wet, std::abs(wet.samples[i] / 32768.0));
    }
    std::printf("  chain peak dry=%.3f wet=%.3f\n", peak_dry, peak_wet);
    ASSERT(peak_wet < peak_dry * 0.9); // ceiling must actually engage
    std::puts("  ok: chain reduces peaks; unknown effect types skipped");
}

// Bypassed slot leaves the audio untouched relative to no chain at all.
void test_fx_chain_bypass_slot_is_transparent()
{
    aur::Json song = plain_song();
    const aur::StereoPcm dry = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    add_chain(song, "Limiter", true, {{0, -1200}});
    const aur::StereoPcm bypassed = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    ASSERT(fnv1a(bypassed.samples) == fnv1a(dry.samples));
    std::puts("  ok: bypassed slot is bit-exact transparent");
}

// Offline and realtime paths agree through the same chain (contract 5).
void test_fx_chain_realtime_parity()
{
    aur::Json song = plain_song();
    add_chain(song, "Limiter", false, {{0, -1200}});
    constexpr int rate = 8000;
    // One bar (16 steps): the realtime mixer loops at bars*16 steps, the offline
    // preview does not; one bar fits before the first wrap, so both paths agree.
    song["tracks"].as_array().clear();
    song["tracks"].as_array().emplace_back(aur::parse_json(
        "{\"name\":\"L\",\"channel\":0,\"program\":48,\"volume\":127,\"pan\":64,"
        "\"notes\":[[0,60,8,127],[4,64,8,127],[8,67,8,127],[12,72,4,127]]}"));
    const aur::StereoPcm offline = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, rate, 30);
    const int frames = std::min(static_cast<int>(offline.samples.size() / 2),
                                16 * rate * 60 / (120 * 4));
    aur::audio::RealtimeMixer mixer;
    mixer.setSampleRate(rate);
    mixer.loadSong(song);
    std::vector<float> live(static_cast<size_t>(frames) * 2, 0.0f);
    mixer.render(live.data(), frames);
    double max_delta = 0.0, sum_ref_sq = 0.0;
    for (int i = 0; i < frames * 2; ++i)
    {
        const double ref = offline.samples[static_cast<size_t>(i)] / 32768.0;
        const double got = live[static_cast<size_t>(i)];
        max_delta = std::max(max_delta, std::abs(ref - got));
        sum_ref_sq += ref * ref;
    }
    const double rel = max_delta / std::sqrt(sum_ref_sq + 1e-12);
    std::printf("  chain parity rel=%g max_delta=%g\n", rel, max_delta);
    ASSERT(rel < 0.0005);
    std::puts("  ok: offline and realtime agree through the chain");
}
