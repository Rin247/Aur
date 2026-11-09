// FX rack suite, ported from reference/synths/FxTest.cpp (frontier-model drop).
// Standalone executable: replaces global operator new to count allocations.
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

#include "aur/audio/CoreState.hpp"
#include "aur/audio/FxRack.hpp"

using namespace aur::audio;

static size_t g_allocs = 0;
void* operator new(size_t n) { ++g_allocs; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

static const double kSr = 44100;
static bool near(float a, float b, float e = 1e-6f) { return std::fabs(a - b) <= e; }

struct Lcg {
    uint32_t s = 12345;
    float next() { s = s * 1664525u + 1013904223u; return float(s >> 8) / 8388608.f - 1.f; }
};
static uint64_t fnv(const std::vector<float>& a) {
    uint64_t h = 1469598103934665603ull;
    for (float f : a) { uint32_t u; std::memcpy(&u, &f, 4); for (int i = 0; i < 4; ++i) { h ^= (u >> (8 * i)) & 0xFF; h *= 1099511628211ull; } }
    return h;
}
static double energy(const std::vector<float>& a, size_t from, size_t to) {
    double e = 0; for (size_t i = from; i < to && i < a.size(); ++i) e += double(a[i]) * a[i];
    return e;
}

static void testDelay() {
    FxDelay d; d.init(kSr);
    d.setTimeMs(100.f, 100.f); d.jumpToTarget(); d.setFeedback(0.5f); d.setDamping(0.f);
    const size_t D = 4410;
    std::vector<float> L(D * 3 + 10);
    for (size_t n = 0; n < L.size(); ++n) { float wl, wr; d.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); L[n] = wl; }
    for (size_t n = 0; n < L.size(); ++n) {
        const float want = n == D ? 1.f : (n == 2 * D ? 0.5f : (n == 3 * D ? 0.25f : 0.f));
        assert(near(L[n], want));
    }
    FxDelay p; p.init(kSr); p.setTimeMs(10.f, 10.f); p.jumpToTarget(); p.setFeedback(0.5f); p.setDamping(0.f); p.setPingPong(true);
    const size_t P = 441;
    std::vector<float> pl, pr;
    for (size_t n = 0; n < P * 3 + 5; ++n) { float wl, wr; p.process(n == 0 ? 1.f : 0.f, n == 0 ? 1.f : 0.f, wl, wr); pl.push_back(wl); pr.push_back(wr); }
    assert(near(pl[P], 1.f) && near(pr[P], 0.f));
    assert(near(pr[2 * P], 0.5f) && near(pl[2 * P], 0.f));
    assert(near(pl[3 * P], 0.25f) && near(pr[3 * P], 0.f));
    FxDelay g; g.init(kSr); g.setTimeMs(50.f, 50.f); g.setFeedback(0.f);
    float wl, wr;
    g.setTimeMs(100.f, 100.f);
    for (int n = 0; n < 44100; ++n) g.process(0.f, 0.f, wl, wr);
    std::vector<float> o;
    for (size_t n = 0; n < D + 2; ++n) { g.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); o.push_back(wl); }
    assert(near(o[D], 1.f));
    std::puts("ok: delay impulse response, ping-pong, time glide");
}

static void testChorus() {
    FxChorus c; c.init(48000.0);
    c.setVoices(1); c.setDepthMs(0.f); c.setDelayMs(2.f); c.setFeedback(0.5f);
    const size_t D = 96;
    std::vector<float> o;
    for (size_t n = 0; n < D * 3 + 5; ++n) { float wl, wr; c.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); o.push_back(wl); }
    assert(near(o[D], 1.f) && near(o[2 * D], 0.5f) && near(o[3 * D], 0.25f) && near(o[D + 1], 0.f));
    for (float spread : {0.f, 1.f}) {
        FxChorus s; s.init(kSr); s.setVoices(3); s.setDepthMs(4.f); s.setRateHz(2.f); s.setSpread(spread);
        double diff = 0;
        for (int n = 0; n < 44100; ++n) {
            const float x = float(std::sin(2 * 3.14159265358979 * 440.0 * n / kSr));
            float wl, wr; s.process(x, x, wl, wr);
            diff += std::fabs(wl - wr);
        }
        if (spread == 0.f) assert(diff == 0.0); else assert(diff > 100.0);
    }
    std::puts("ok: chorus/flanger impulse response, stereo spread");
}

static void testReverb() {
    FxReverb r; r.init(kSr); r.setRoomSize(0.5f); r.setDamping(0.4f); r.setWidth(1.f);
    std::vector<float> L, R;
    for (size_t n = 0; n < size_t(kSr * 4); ++n) { float wl, wr; r.process(n == 0 ? 1.f : 0.f, n == 0 ? 1.f : 0.f, wl, wr); L.push_back(wl); R.push_back(wr); }
    const size_t first = size_t(0.0297 * kSr) + 1;
    for (size_t n = 0; n < first; ++n) assert(L[n] == 0.f);
    assert(energy(L, first, first + 3000) > 0.0);
    const double e1 = energy(L, size_t(0.5 * kSr), size_t(0.6 * kSr)), e2 = energy(L, size_t(3.0 * kSr), size_t(3.1 * kSr));
    assert(e1 > e2 * 100.0);
    double diff = 0; for (size_t n = 0; n < L.size(); ++n) diff += std::fabs(L[n] - R[n]);
    assert(diff > 0.0);

    auto tail = [](float room) {
        FxReverb q; q.init(kSr); q.setRoomSize(room); q.setDamping(0.4f);
        std::vector<float> o;
        for (size_t n = 0; n < size_t(kSr * 2); ++n) { float wl, wr; q.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); o.push_back(wl); }
        return energy(o, size_t(1.5 * kSr), size_t(1.6 * kSr));
    };
    assert(tail(1.0f) > tail(0.2f) * 50.0);

    FxReverb pd; pd.init(kSr); pd.setPreDelayMs(50.f);
    std::vector<float> o;
    for (size_t n = 0; n < 8000; ++n) { float wl, wr; pd.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); o.push_back(wl); }
    for (size_t n = 0; n < size_t(0.0797 * kSr); ++n) assert(o[n] == 0.f);

    FxReverb w0; w0.init(kSr); w0.setWidth(0.f);
    for (size_t n = 0; n < 5000; ++n) { float wl, wr; w0.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); assert(wl == wr); }

    FxReverb m; m.init(kSr); m.setRoomSize(1.f); m.setDamping(0.f);
    Lcg g; float pk = 0;
    for (size_t n = 0; n < size_t(kSr * 3); ++n) {
        float wl, wr; const float x = n < 44100 ? g.next() : 0.f;
        m.process(x, -x, wl, wr);
        assert(std::isfinite(wl) && std::isfinite(wr));
        pk = std::max(pk, std::max(std::fabs(wl), std::fabs(wr)));
    }
    assert(pk < 4.f);
    m.reset();
    for (int n = 0; n < 1000; ++n) { float wl, wr; m.process(0.f, 0.f, wl, wr); assert(wl == 0.f && wr == 0.f); }
    std::puts("ok: reverb onset, decay, room size, pre-delay, width, stability");
}

static void testRack() {
    FxRack r; r.init(kSr);
    assert(!r.enabled());
    Lcg g;
    std::vector<float> l(2048), rr(2048);
    for (size_t i = 0; i < l.size(); ++i) { l[i] = g.next(); rr[i] = g.next(); }
    auto l0 = l, r0 = rr;
    r.processBlock(l.data(), rr.data(), l.size());
    assert(l == l0 && rr == r0);

    r.setParam(FxParam::Enable, 1);
    r.processBlock(l.data(), rr.data(), l.size());
    assert(l == l0 && rr == r0);

    r.setParam(FxParam::MasterSendReverb, 60); r.setParam(FxParam::MasterSendDelay, 40); r.setParam(FxParam::MasterSendChorus, 50);
    std::vector<float> ll(4410, 0.f), rl(4410, 0.f);
    ll[0] = rl[0] = 1.f;
    r.processBlock(ll.data(), rl.data(), ll.size());
    assert(energy(ll, 100, 4410) > 0.0);

    auto run = [&](int c2r) {
        FxRack q; q.init(kSr); q.setParam(FxParam::Enable, 1); q.setParam(FxParam::ChorusToReverb, c2r);
        q.setParam(FxParam::ChorusLevel, 0);
        std::vector<float> o;
        for (size_t n = 0; n < 20000; ++n) {
            float ol, orr; const float x = n == 0 ? 1.f : 0.f;
            q.processSample(0.f, 0.f, x, x, 0.f, 0.f, 0.f, 0.f, ol, orr);
            o.push_back(ol);
        }
        return energy(o, 0, o.size());
    };
    assert(run(0) == 0.0 && run(100) > 0.0);

    for (size_t i = 0; i < kFxParamTable.size(); ++i) {
        const auto& t = kFxParamTable[i];
        assert(t.name && t.min <= t.def && t.def <= t.max);
        FxRack q; q.init(kSr);
        assert(q.param(FxParam(i)) == t.def);
        q.setParam(FxParam(i), 100000); assert(q.param(FxParam(i)) == t.max);
        q.setParam(FxParam(i), -100000); assert(q.param(FxParam(i)) == t.min);
    }
    std::puts("ok: rack bypass exactness, sends, routing, parameter table");
}

static std::vector<float> rackScene(FxRack& r, int frames, uint32_t seed) {
    Lcg g; g.s = seed;
    std::vector<float> out;
    for (int n = 0; n < frames; ++n) {
        const float x = (n % 3000 < 200) ? g.next() * 0.5f : 0.f;
        float ol, orr;
        r.processSample(x, x, x * 0.5f, x * 0.5f, x * 0.5f, x * 0.5f, x * 0.5f, x * 0.5f, ol, orr);
        out.push_back(ol); out.push_back(orr);
    }
    return out;
}
static void setupRack(FxRack& r) {
    r.setParam(FxParam::Enable, 1); r.setParam(FxParam::DelayPingPong, 1); r.setParam(FxParam::ChorusFeedback, 30);
    r.setParam(FxParam::ChorusToReverb, 30); r.setParam(FxParam::DelayToReverb, 20); r.setParam(FxParam::ChorusVoices, 4);
}

static void testDeterminismStateAlloc() {
    FxRack a; a.init(kSr); setupRack(a);
    FxRack b; b.init(kSr); setupRack(b);
    assert(fnv(rackScene(a, 44100, 7)) == fnv(rackScene(b, 44100, 7)));

    FxRack c; c.init(kSr); setupRack(c);
    const size_t n0 = g_allocs;
    for (int i = 0; i < 20000; ++i) {
        float ol, orr; c.processSample(0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, ol, orr);
        if (i % 4000 == 0) c.setParam(FxParam::DelayTimeL, 100 + i / 100);
    }
    assert(g_allocs == n0);

    FxRack s; s.init(kSr); setupRack(s); rackScene(s, 9000, 3);
    StateSizer sz; s.visit(sz);
    std::vector<uint8_t> blob(sz.size());
    StateWriter w(blob.data(), blob.size()); s.visit(w);
    assert(w.ok() && w.size() == blob.size());
    const uint64_t cont = fnv(rackScene(s, 20000, 11));
    FxRack t; t.init(kSr);
    StateReader rd(blob.data(), blob.size()); t.visit(rd);
    assert(rd.ok());
    assert(fnv(rackScene(t, 20000, 11)) == cont);
    std::puts("ok: FX determinism, zero allocation, state round-trip");
}

int main() {
    testDelay(); testChorus(); testReverb(); testRack(); testDeterminismStateAlloc();
    std::puts("FX: ALL PASSED");
    return 0;
}
