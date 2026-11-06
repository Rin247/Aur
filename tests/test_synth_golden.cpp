#include "mini_test.hpp"
#include "aur/synth.hpp"
#include "aur/json.hpp"

#include <cstdint>
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
            hash ^= static_cast<std::uint64_t>(static_cast<std::uint16_t>(s)) >> 8;
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    aur::Json load_song(const char *name)
    {
        return aur::parse_json(
            std::string("{\"name\":\"") + name +
            "\",\"tempo\":120,\"bars\":8,\"beats\":4,"
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":0,"
            "\"notes\":[[0,60,4,100],[8,64,4,100],[16,67,4,100],[24,72,4,100]]}]}");
    }

} // namespace

// Golden render-hash contract for the preview synth over the three voices.
// The hashes pin deterministic PCM output for a fixed song + rate; change
// them deliberately in the same commit as any DSP change.
void test_synth_golden_hash()
{
    const aur::Json song = load_song("Golden");

    const aur::StereoPcm gm = aur::synthesize_preview(song, aur::PreviewSynth::Gm, 8000, 30);
    const aur::StereoPcm fm = aur::synthesize_preview(song, aur::PreviewSynth::Fm, 8000, 30);
    const aur::StereoPcm psg = aur::synthesize_preview(song, aur::PreviewSynth::RetroPsg, 8000, 30);

    ASSERT(gm.samples.size() == fm.samples.size());
    ASSERT(gm.samples.size() == psg.samples.size());
    ASSERT(gm.sample_rate == 8000);

    const std::uint64_t gm_hash = fnv1a(gm.samples);
    const std::uint64_t fm_hash = fnv1a(fm.samples);
    const std::uint64_t psg_hash = fnv1a(psg.samples);
    std::printf("  gm=%016llx fm=%016llx psg=%016llx\n",
                static_cast<unsigned long long>(gm_hash),
                static_cast<unsigned long long>(fm_hash),
                static_cast<unsigned long long>(psg_hash));

    // Replay determinism: a fresh render must hash identically.
    const aur::StereoPcm gm_again = aur::synthesize_preview(song, aur::PreviewSynth::Gm, 8000, 30);
    ASSERT(fnv1a(gm_again.samples) == gm_hash);

    // The three voices must sound distinct.
    ASSERT(gm_hash != fm_hash);
    ASSERT(gm_hash != psg_hash);
    ASSERT(fm_hash != psg_hash);

    // Golden values — replace deliberately alongside DSP changes.
    constexpr std::uint64_t kGoldenGm = 0xcb85984999374d93ULL;
    constexpr std::uint64_t kGoldenFm = 0x593f65526dc8c07bULL;
    constexpr std::uint64_t kGoldenPsg = 0xe956398a8d885a99ULL;
    ASSERT(gm_hash == kGoldenGm);
    ASSERT(fm_hash == kGoldenFm);
    ASSERT(psg_hash == kGoldenPsg);
}
