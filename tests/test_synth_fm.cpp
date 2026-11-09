// Build: g++ -std=c++20 -O2 -Wall -Wextra YmFmCoreTest.cpp YmFmCore.cpp YmFmParams.cpp YmFmPatches.cpp FxRack.cpp FxChorusDelay.cpp FxReverb.cpp -o YmFmCoreTest
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

#pragma GCC diagnostic ignored "-Wmismatched-new-delete"  // test replaces operator new to count allocations
#include "aur/audio/YmFmCore.hpp"
#include "aur/audio/YmFmPatches.hpp"

using namespace aur::audio;

static size_t g_allocs = 0;
void* operator new(size_t n) { ++g_allocs; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

static constexpr double kEgSr = 53267.0 / 3.0;  // one EG tick per sample: lets tests count ticks exactly

static uint64_t fnv(const std::vector<float>& a, uint64_t h = 1469598103934665603ull) {
    for (float f : a) { uint32_t u; std::memcpy(&u, &f, 4); for (int i = 0; i < 4; ++i) { h ^= (u >> (8 * i)) & 0xFF; h *= 1099511628211ull; } }
    return h;
}
static bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

static YmChannelParams simplePatch(int alg) {
    YmChannelParams p; p.algorithm = alg;
    for (auto& o : p.op) { o.tl = 127; o.ar = 31; o.rr = 7; }
    return p;
}

static void testAlgorithms() {
    const float amp[4] = {1.0f, 0.8f, 0.6f, 0.9f}, ph[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    // Hand-derived with an independent graph (see header diagram): fb=0.1, modScale=0.5.
    const float expected[8] = {-0.017327760f, -0.292063981f, -0.183738600f, 0.325355330f,
                               -1.315060528f, -1.911757832f, 0.611269456f, 2.669502844f};
    for (int a = 0; a < 8; ++a) {
        float o1 = 0;
        const float y = ymEvalAlgorithm(a, amp, ph, 0.1f, 0.5f, &o1);
        assert(near(y, expected[a], 2e-5f));
        assert(near(o1, 0.809016994f, 2e-5f));
    }
    // Trivial exact cases: only op1 audible at phase 0.25 -> sin = 1 in every algorithm that outputs op1 directly.
    const float a1[4] = {1, 0, 0, 0}, p1[4] = {0.25f, 0, 0, 0};
    assert(near(ymEvalAlgorithm(7, a1, p1, 0, 4.0f, nullptr), 1.0f));
    assert(near(ymEvalAlgorithm(0, a1, p1, 0, 4.0f, nullptr), 0.0f));  // op4 amp 0 -> silent chain
    assert(ymCarrierMask(0) == 8 && ymCarrierMask(4) == 10 && ymCarrierMask(7) == 15);
    assert(ymKeyCode(12) == 0 && ymKeyCode(60) == 16 && ymKeyCode(127) == 31);
    std::puts("ok: algorithms");
}

static void testEnvelope() {
    YmFmCore c; c.init(kEgSr);
    YmChannelParams p = simplePatch(7);
    for (auto& o : p.op) { o.tl = 0; o.d1r = 20; o.d1l = 15; o.rs = 0; o.rr = 15; }
    c.setChannelParams(0, p);
    c.noteOn(0, 60, 127);
    std::vector<float> l(1024), r(1024);
    c.render(l.data(), r.data(), 801);
    // sample 1: attack (rate>=62) -> env 0; then 800 decay ticks at rate 42 -> 0.375 units/tick = 300.
    assert(c.operatorAttenuation(0, 3) == 300);
    // release with RR=15 reaches silence within ~80 ticks and frees the channel
    c.noteOff(0, 60);
    c.render(l.data(), r.data(), 100);
    assert(c.operatorAttenuation(0, 3) == 1023);
    // AR=0 never attacks
    YmFmCore d; d.init(kEgSr);
    YmChannelParams q = simplePatch(7); q.op[3].ar = 0; q.op[3].tl = 0;
    d.setChannelParams(0, q); d.noteOn(0, 60, 127);
    d.render(l.data(), r.data(), 500);
    assert(d.operatorAttenuation(0, 3) == 1023);
    std::puts("ok: envelope timing");
}

static void testSsg() {
    auto run = [](int ssg, int samples, std::vector<int>& att) {
        YmFmCore c; c.init(kEgSr);
        YmChannelParams p = simplePatch(7);
        for (auto& o : p.op) { o.tl = 0; o.d1r = 31; o.d1l = 15; }
        p.op[3].ssg = ssg;
        c.setChannelParams(0, p); c.noteOn(0, 60, 127);
        std::vector<float> l(1), r(1);
        for (int i = 0; i < samples; ++i) { c.render(l.data(), r.data(), 1); att.push_back(c.operatorAttenuation(0, 3)); }
    };
    std::vector<int> a;
    run(8 | 1, 200, a);  assert(a.back() == 1023 && a[150] == 1023);  // "\___" hold silent
    a.clear(); run(8 | 3, 200, a);  assert(a.back() == 0);            // "\---" hold loud
    a.clear(); run(8 | 5, 200, a);  assert(a.back() == 0 && a[3] > 400);  // "/---" starts quiet, ends loud
    a.clear(); run(8 | 7, 200, a);  assert(a.back() == 1023);         // "/___"
    a.clear(); run(8, 400, a);                                         // "\\\\" saw: repeated reset
    int resets = 0;
    for (size_t i = 1; i < a.size(); ++i) if (a[i - 1] > 450 && a[i] < 50) ++resets;
    assert(resets >= 3);
    a.clear(); run(8 | 2, 400, a);                                     // triangle: output reverses direction
    int rises = 0, falls = 0;
    for (size_t i = 1; i < a.size(); ++i) { if (a[i] > a[i - 1] + 3) ++rises; if (a[i] < a[i - 1] - 3) ++falls; }
    assert(rises > 5 && falls > 5);
    std::puts("ok: SSG-EG shapes");
}

static void testChannelsAndPan() {
    YmFmCore c; c.init(44100);
    for (int ch = 0; ch < kYmChannels; ++ch) { c.loadBuiltinPatch(ch, ch); c.noteOn(ch, 48 + ch * 3, 100); }
    c.noteOn(6, 60, 100); c.noteOn(-1, 60, 100);  // out of range ignored
    for (int ch = 0; ch < kYmChannels; ++ch) assert(c.keyedOn(ch));
    c.noteOff(0, 99);  assert(c.keyedOn(0));        // mismatched note ignored
    c.noteOff(0, 48);  assert(!c.keyedOn(0));
    c.noteOn(1, 70, 90);                            // retrigger same channel (monophonic)
    assert(c.keyedOn(1));
    std::vector<float> l(4410), r(4410);
    c.render(l.data(), r.data(), 4410);
    float pk = 0; for (float v : l) pk = std::max(pk, std::fabs(v));
    assert(pk > 0.01f);

    YmFmCore p; p.init(44100);
    p.loadBuiltinPatch(0, 0); p.setChannelParam(0, YmParam::Pan, 2);  // left only
    p.noteOn(0, 48, 120); p.render(l.data(), r.data(), 4410);
    float pl = 0, pr = 0; for (size_t i = 0; i < l.size(); ++i) { pl = std::max(pl, std::fabs(l[i])); pr = std::max(pr, std::fabs(r[i])); }
    assert(pl > 0.01f && pr == 0.f);
    p.setChannelParam(0, YmParam::Pan, 3); p.controlChange(0, 10, 127);  // CC10 hard right
    p.noteOn(0, 48, 120); p.render(l.data(), r.data(), 4410);
    pl = pr = 0; for (size_t i = 0; i < l.size(); ++i) { pl = std::max(pl, std::fabs(l[i])); pr = std::max(pr, std::fabs(r[i])); }
    assert(pr > 0.01f && pl == 0.f);
    std::puts("ok: 6-channel monophony, pan");
}

static void testDacAndSustain() {
    YmFmCore c; c.init(44100);
    uint8_t ramp[64]; for (int i = 0; i < 64; ++i) ramp[i] = uint8_t(128 + i * 2);
    c.setDacBuffer(ramp, 64, 44100.0);
    c.setGlobalParam(YmParam::DacEnable, 1);
    c.noteOn(5, 60, 127);
    std::vector<float> l(80), r(80);
    c.render(l.data(), r.data(), 80);
    const float g = std::pow(100.f / 127.f, 2.f);  // CC7 default 100, squared
    for (int i = 0; i < 64; ++i) assert(near(l[size_t(i)], (ramp[i] - 128.f) / 128.f * 0.5f * g * 0.5f, 1e-5f));
    assert(l[70] == 0.f);  // buffer ended
    // sustain pedal defers release
    YmFmCore s; s.init(44100); s.loadBuiltinPatch(0, 4);
    s.controlChange(0, 64, 127); s.noteOn(0, 60, 100); s.noteOff(0, 60);
    assert(s.keyedOn(0));
    s.controlChange(0, 64, 0);
    assert(!s.keyedOn(0));
    std::puts("ok: DAC mode, sustain pedal");
}

static void testPatchesAndParams() {
    for (int i = 0; i < kYmBuiltinPatchCount; ++i) {
        YmFmCore c; c.init(44100);
        assert(c.loadBuiltinPatch(0, i));
        uint8_t raw[kYmSysexBytes];
        assert(YmFmCore::patchToBytes(c.channelParams(0), raw, true) == size_t(kYmSysexBytes));
        YmFmCore d; d.init(44100);
        assert(d.loadPatchBytes(3, raw));
        uint8_t raw2[kYmPatchBytes];
        YmFmCore::patchToBytes(d.channelParams(3), raw2, false);
        assert(std::memcmp(raw + 5, raw2, kYmPatchBytes) == 0);
        c.noteOn(0, 55, 110);
        std::vector<float> l(22050), r(22050);
        c.render(l.data(), r.data(), 22050);
        float pk = 0; for (float v : l) { assert(std::isfinite(v)); pk = std::max(pk, std::fabs(v)); }
        assert(pk > 0.005f);
    }
    assert(kYmBuiltinPatchCount >= 12);
    YmFmCore c; c.init(44100);
    uint8_t raw[kYmSysexBytes];
    YmFmCore::patchToBytes(ymBuiltinPatch(0).params, raw, true);
    raw[53] ^= 1; assert(!c.loadPatchBytes(0, raw));             // bad checksum
    raw[53] ^= 1; raw[10] = 99; assert(!c.loadPatchBytes(0, std::span<const uint8_t>(raw + 5, kYmPatchBytes)));  // out of range
    // param table matches reset defaults and setters clamp
    for (size_t i = 0; i < kYmParamTable.size(); ++i) {
        const auto& t = kYmParamTable[i];
        assert(t.min <= t.def && t.def <= t.max && t.name);
        const YmParam p = YmParam(i);
        if (t.scope == YmParamScope::Global) { assert(c.globalParam(p) == t.def); c.setGlobalParam(p, 9999); assert(c.globalParam(p) == t.max); }
        else if (t.scope == YmParamScope::Channel) { assert(c.channelParam(2, p) == t.def); c.setChannelParam(2, p, -50); assert(c.channelParam(2, p) == t.min); }
        else { assert(c.operatorParam(2, 1, p) == t.def); c.setOperatorParam(2, 1, p, 9999); assert(c.operatorParam(2, 1, p) == t.max); }
    }
    assert(!c.setChannelParam(0, YmParam::OpTl, 1) && !c.setOperatorParam(9, 0, YmParam::OpTl, 1));
    std::puts("ok: patches, sysex frame, parameter table");
}

static std::vector<float> renderScene(YmFmCore& c, int frames) {
    const size_t nf = size_t(frames);
    std::vector<float> l(nf), r(nf), out;
    c.render(l.data(), r.data(), nf);
    out = l; out.insert(out.end(), r.begin(), r.end());
    return out;
}
static void setupScene(YmFmCore& c) {
    c.setGlobalParam(YmParam::LfoEnable, 1); c.setGlobalParam(YmParam::LfoRate, 4);
    c.fx().setParam(FxParam::Enable, 1);
    for (int ch = 0; ch < kYmChannels; ++ch) {
        c.loadBuiltinPatch(ch, ch + 1); c.setChannelParam(ch, YmParam::Fms, 3); c.setChannelParam(ch, YmParam::Ams, 2);
        c.setOperatorParam(ch, 3, YmParam::OpAm, 1); c.controlChange(ch, 1, 60);
        c.controlChange(ch, 91, 40 + ch * 10); c.controlChange(ch, 93, 30); c.controlChange(ch, 94, 20);
        c.noteOn(ch, 40 + ch * 5, 90 + ch);
    }
}

static void testDeterminismStateAlloc() {
    YmFmCore a; a.init(44100); setupScene(a);
    const size_t before = g_allocs;
    const uint64_t h1 = fnv(renderScene(a, 44100));
    (void)before;
    YmFmCore b; b.init(44100); setupScene(b);
    assert(h1 == fnv(renderScene(b, 44100)));

    // allocation check: render into pre-sized buffers only
    std::vector<float> l(512), r(512);
    YmFmCore c; c.init(44100); setupScene(c);
    const size_t n0 = g_allocs;
    for (int i = 0; i < 50; ++i) { c.render(l.data(), r.data(), 512); c.noteOn(i % 6, 50 + i % 20, 100); c.noteOff((i + 1) % 6, 55); c.controlChange(i % 6, 74, i); c.pitchBend(i % 6, 8192 + i * 40); }
    assert(g_allocs == n0);

    // state save/load: restored instance continues bit-identically
    YmFmCore s; s.init(44100); setupScene(s); renderScene(s, 3000);
    std::vector<uint8_t> blob(s.stateSize());
    size_t written = 0;
    assert(s.saveState(blob.data(), blob.size(), &written) && written == blob.size());
    const uint64_t cont = fnv(renderScene(s, 5000));
    YmFmCore t; t.init(44100);
    assert(t.loadState(blob.data(), blob.size()));
    assert(fnv(renderScene(t, 5000)) == cont);
    assert(!t.loadState(blob.data(), blob.size() - 1));  // truncated rejected, state untouched
    assert(fnv(renderScene(t, 100)) == fnv(renderScene(s, 100)));
    std::puts("ok: determinism, zero allocation, state round-trip");
}

static void testLfo() {
    YmFmCore on, off; on.init(44100); off.init(44100);
    for (YmFmCore* c : {&on, &off}) { c->loadBuiltinPatch(0, 8); c->setChannelParam(0, YmParam::Fms, 7); c->noteOn(0, 60, 120); }
    on.setGlobalParam(YmParam::LfoEnable, 1); on.setGlobalParam(YmParam::LfoRate, 7);
    assert(fnv(renderScene(on, 4410)) != fnv(renderScene(off, 4410)));
    std::puts("ok: LFO");
}

static void testFx() {
    auto run = [](int enable, int cc91) {
        YmFmCore c; c.init(44100);
        c.fx().setParam(FxParam::Enable, enable); c.fx().setParam(FxParam::ReverbLevel, 100);
        c.loadBuiltinPatch(0, 3); c.controlChange(0, 91, cc91);
        c.noteOn(0, 55, 110);
        std::vector<float> a = renderScene(c, 6000);
        c.noteOff(0, 55);
        std::vector<float> b = renderScene(c, 40000);
        a.insert(a.end(), b.begin(), b.end());
        return a;
    };
    const auto off = run(0, 0), onZero = run(1, 0), onSend = run(1, 127);
    assert(off == onZero && off != onSend);   // zero send = bit-exact passthrough, CC91 adds a tail
    double tOff = 0, tOn = 0;
    for (size_t i = 60000; i < 86000; ++i) { tOff += double(off[i]) * off[i]; tOn += double(onSend[i]) * onSend[i]; }
    assert(tOn > tOff * 5.0);
    std::puts("ok: FX rack integration (CC91 send, exact bypass)");
}

int main() {
    testFx();
    testAlgorithms(); testEnvelope(); testSsg(); testChannelsAndPan(); testDacAndSustain();
    testPatchesAndParams(); testDeterminismStateAlloc(); testLfo();
    std::puts("YmFmCore: ALL PASSED");
    return 0;
}
