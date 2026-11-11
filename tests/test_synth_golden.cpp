#include "mini_test.hpp"
#include "aur/audio/synth.hpp"

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

// Golden render-hash contract for the preview synths. The hashes pin
// deterministic PCM output; change them in the same commit as any DSP change.
void test_synth_golden_hash()
{
    const aur::Json song = load_song("Golden");

    const aur::StereoPcm md = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    const aur::StereoPcm spc = aur::synthesize_preview(song, aur::PreviewSynth::Spc, 8000, 30);

    ASSERT(md.sample_rate == 8000);
    ASSERT(md.samples.size() == spc.samples.size());
    ASSERT(!md.samples.empty());

    const std::uint64_t md_hash = fnv1a(md.samples);
    const std::uint64_t spc_hash = fnv1a(spc.samples);
    std::printf("  mdfm=%016llx spc=%016llx\n",
                static_cast<unsigned long long>(md_hash),
                static_cast<unsigned long long>(spc_hash));

    // Replay determinism: a fresh render must hash identically.
    const aur::StereoPcm md_again = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 30);
    ASSERT(fnv1a(md_again.samples) == md_hash);

    // The two cores must sound distinct.
    ASSERT(md_hash != spc_hash);

    // Golden values — replace deliberately alongside DSP changes.
    constexpr std::uint64_t kGoldenMdFm = 0x046ead640c7d4e21ULL;
    constexpr std::uint64_t kGoldenSpc = 0x584d613594010ef4ULL;
    ASSERT(md_hash == kGoldenMdFm);
    ASSERT(spc_hash == kGoldenSpc);
}
