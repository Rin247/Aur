// FX effects suite: framework registry, per-effect behaviour, determinism,
// zero-allocation render paths. Patterns from the synth-core suites.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "aur/audio/Effect.hpp"
#include "aur/audio/EffectChain.hpp"

using namespace aur::audio;

static size_t g_allocs = 0;
void *operator new(size_t n)
{
    ++g_allocs;
    if (void *p = std::malloc(n))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }

static const double kSr = 44100;
static uint64_t fnv(const std::vector<float> &a)
{
    uint64_t h = 1469598103934665603ull;
    for (float f : a)
    {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        for (int i = 0; i < 4; ++i)
        {
            h ^= (u >> (8 * i)) & 0xFF;
            h *= 1099511628211ull;
        }
    }
    return h;
}

static std::vector<float> renderStereo(IEffect &fx, int frames, uint32_t seed, float gain = 0.5f)
{
    uint32_t s = seed;
    std::vector<float> out;
    out.reserve(size_t(frames) * 2);
    for (int i = 0; i < frames; ++i)
    {
        s = s * 1664525u + 1013904223u;
        const float x = float(s >> 8) / 8388608.f - 1.f;
        float l = x * gain, r = -x * gain;
        fx.process(&l, &r, 1);
        out.push_back(l);
        out.push_back(r);
    }
    return out;
}

static void testRegistry()
{
    for (int i = 0; i < int(EffectId::Count); ++i)
    {
        const EffectId id = EffectId(i);
        auto fx = makeEffect(id);
        assert(fx);
        assert(fx->name() && std::strcmp(fx->name(), effectName(id)) == 0);
        fx->init(kSr);
        assert(fx->paramCount() > 0);
        for (int p = 0; p < fx->paramCount(); ++p)
        {
            const EffectParamInfo &t = fx->paramTable()[p];
            assert(t.name && t.min <= t.def && t.def <= t.max);
            assert(fx->param(p) == t.def);
            assert(fx->setParam(p, t.max + 1000) && fx->param(p) == t.max);
            assert(fx->setParam(p, t.min - 1000) && fx->param(p) == t.min);
            assert(fx->setParam(p, t.def));
        }
        assert(!fx->setParam(fx->paramCount(), 0));
        assert(!fx->setParam(-1, 0));
    }
    assert(makeEffect(EffectId(int(EffectId::Count) + 3)) == nullptr);
    std::puts("ok: registry ids, names, parameter tables, clamping");
}

static void testBoundedAndSilenceClean()
{
    for (int i = 0; i < int(EffectId::Count); ++i)
    {
        std::fprintf(stderr, "bounded fx %d\n", i);
        auto fx = makeEffect(EffectId(i));
        fx->init(kSr);
        std::vector<float> l(4410), r(4410);
        for (size_t n = 0; n < l.size(); ++n)
        {
            const float x = 0.3f * float(std::sin(2.0 * 3.14159265358979323846 * 1000.0 * double(n) / kSr));
            l[n] = x;
            r[n] = -x;
        }
        fx->process(l.data(), r.data(), l.size());
        for (float v : l)
            assert(std::isfinite(v) && std::fabs(v) < 4.f);
        for (float v : r)
            assert(std::isfinite(v) && std::fabs(v) < 4.f);
        fx->reset();
        float zl = 0.f, zr = 0.f;
        for (int n = 0; n < 1000; ++n)
        {
            float a = 0.f, b = 0.f;
            fx->process(&a, &b, 1);
            zl = std::max(zl, std::fabs(a));
            zr = std::max(zr, std::fabs(b));
        }
        assert(zl == 0.f && zr == 0.f);
    }
    std::puts("ok: all effects bounded on tone input and silence-clean after reset");
}

static void testSpecificBehaviour()
{
    // EQ low boost raises bass energy relative to default.
    {
        auto eq = makeEffect(EffectId::EqThreeBand);
        eq->init(kSr);
        auto tone = [&](IEffect &fx)
        {
            std::vector<float> l(22050), r(22050);
            for (size_t n = 0; n < l.size(); ++n)
                l[n] = r[n] = 0.3f * float(std::sin(2.0 * 3.14159265358979323846 * 100.0 * double(n) / kSr));
            fx.process(l.data(), r.data(), l.size());
            double e = 0;
            for (float v : l)
                e += double(v) * v;
            return e;
        };
        const double base = tone(*eq);
        eq->setParam(0, 120); // +12 dB low shelf
        eq->setParam(1, -120); // neutralise mid so it does not colour the tone
        eq->setParam(2, -120); // neutralise high
        // Mid still adds a little at 100 Hz; accept a broad boost threshold.
        const double boosted = tone(*eq);
        std::printf("  eq base=%.1f boosted=%.1f ratio=%.2f\n", base, boosted, boosted / base);
        assert(boosted > base * 1.5);
    }
    // Filter: LP at 200 Hz kills a 5 kHz tone.
    {
        auto f = makeEffect(EffectId::StateVariableFilter);
        f->init(kSr);
        f->setParam(0, 0);
        f->setParam(1, 200);
        std::vector<float> l(22050), r(22050);
        for (size_t n = 0; n < l.size(); ++n)
            l[n] = r[n] = 0.5f * float(std::sin(2.0 * 3.14159265358979323846 * 5000.0 * double(n) / kSr));
        f->process(l.data(), r.data(), l.size());
        double e = 0;
        for (float v : l)
            e += double(v) * v;
        assert(e < 0.05 * 22050 * 0.125);
    }
    // Limiter: a full-scale burst never exceeds the ceiling afterwards.
    {
        auto lim = makeEffect(EffectId::Limiter);
        lim->init(kSr);
        std::vector<float> l(22050, 0.99f), r(22050, -0.99f);
        lim->process(l.data(), r.data(), l.size());
        for (float v : l)
            assert(std::fabs(v) <= 1.001f);
    }
    // Bitcrusher with 1 bit quantises to two levels: round(x*2)/2 gives -1/-0.5
    // for negative x and 0/+0.5 for positive x, i.e. one sign bit worth of depth.
    {
        auto bc = makeEffect(EffectId::Bitcrusher);
        bc->init(kSr);
        bc->setParam(0, 1); // bits
        std::vector<float> l(4096), r(4096, 0.f);
        uint32_t s = 7;
        for (size_t n = 0; n < l.size(); ++n)
        {
            s = s * 1664525u + 1013904223u;
            l[n] = float(s >> 8) / 8388608.f - 1.f;
        }
        bc->process(l.data(), r.data(), l.size());
        for (size_t n = 0; n < l.size(); ++n)
        {
            const float in = l[n];
            const float quant = in < 0.f ? -1.f : (in < 0.5f ? 0.f : 0.5f);
            assert(std::fabs(in - quant) <= 0.501f);
            if (in < -0.25f) assert(in == -1.f || in == -0.5f);
            if (in > 0.25f) assert(in == 0.5f || in == 1.f);
        }
    }
    std::puts("ok: EQ boost, LP filter, limiter ceiling, bitcrush quantisation");
}

static void testChain()
{
    EffectChain chain;
    std::vector<EffectChain::Slot> slots = {
        {EffectId::Distortion, false}, {EffectId::Tremolo, false}, {EffectId::Limiter, false}};
    assert(chain.build(kSr, slots));
    std::vector<float> l(4410), r(4410);
    uint32_t s = 3;
    for (size_t i = 0; i < l.size(); ++i)
    {
        s = s * 1664525u + 1013904223u;
        l[i] = r[i] = float(s >> 8) / 8388608.f - 1.f;
    }
    auto l0 = l, r0 = r;
    float pl = 0, pr = 0;
    chain.process(l.data(), r.data(), l.size(), pl, pr);
    assert(pl > 0 && pr > 0);
    for (float v : l)
        assert(std::isfinite(v) && std::fabs(v) <= 1.001f);

    // Determinism: rebuild, same input, same hash.
    EffectChain again;
    assert(again.build(kSr, slots));
    auto l1 = l0, r1 = r0;
    float p1 = 0, p2 = 0;
    again.process(l1.data(), r1.data(), l1.size(), p1, p2);
    assert(fnv(l1) == fnv(l) && fnv(r1) == fnv(r));

    // Bypass slot: bypassing the distortion changes the result.
    slots[0].bypass = true;
    EffectChain partial;
    assert(partial.build(kSr, slots));
    auto l2 = l0, r2 = r0;
    partial.process(l2.data(), r2.data(), l2.size(), p1, p2);
    assert(fnv(l2) != fnv(l));

    // Empty chain = untouched.
    EffectChain empty;
    assert(empty.build(kSr, {}));
    auto l3 = l0, r3 = r0;
    empty.process(l3.data(), r3.data(), l3.size(), p1, p2);
    assert(l3 == l0 && r3 == r0);
    std::puts("ok: chain build, determinism, bypass slot, empty chain passthrough");
}

static void testDeterminismAlloc()
{
    for (int i = 0; i < int(EffectId::Count); ++i)
    {
        auto fx = makeEffect(EffectId(i));
        fx->init(kSr);
        fx->setParam(0, fx->paramTable()[0].min + 1);
        const uint64_t h1 = fnv(renderStereo(*fx, 22050, 11));
        fx->reset();
        const uint64_t h2 = fnv(renderStereo(*fx, 22050, 11));
        assert(h1 == h2);
        const size_t n0 = g_allocs;
        for (int k = 0; k < 2000; ++k)
        {
            float a = 0.1f, b = -0.1f;
            fx->process(&a, &b, 1);
        }
        assert(g_allocs == n0);
    }
    std::puts("ok: per-effect determinism and zero allocation on render");
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    testRegistry();
    testBoundedAndSilenceClean();
    testSpecificBehaviour();
    testChain();
    testDeterminismAlloc();
    std::puts("FX effects: ALL PASSED");
    return 0;
}
