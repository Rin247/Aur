// Build: g++ -std=c++20 -O2 -Wall -Wextra SnesPcmCoreTest.cpp SnesPcmCore.cpp SnesSamples.cpp BrrCodec.cpp SnesEcho.cpp FxRack.cpp FxChorusDelay.cpp FxReverb.cpp -o SnesPcmCoreTest
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
#include "aur/audio/BrrCodec.hpp"
#include "aur/audio/SnesEcho.hpp"
#include "aur/audio/SnesPcmCore.hpp"

using namespace aur::audio;

static size_t g_allocs = 0;
void* operator new(size_t n) { ++g_allocs; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

static bool near(float a, float b, float e = 1e-6f) { return std::fabs(a - b) <= e; }
static uint64_t fnv(const std::vector<float>& a) {
    uint64_t h = 1469598103934665603ull;
    for (float f : a) { uint32_t u; std::memcpy(&u, &f, 4); for (int i = 0; i < 4; ++i) { h ^= (u >> (8 * i)) & 0xFF; h *= 1099511628211ull; } }
    return h;
}
static void packBlock(uint8_t* b, int header, const int* nibs) {
    b[0] = uint8_t(header);
    for (int i = 0; i < 16; i += 2) b[1 + i / 2] = uint8_t(((nibs[i] & 15) << 4) | (nibs[i + 1] & 15));
}

static void testBrrDecode() {
    uint8_t blk[9];
    int16_t out[16];
    BrrFilterState st;
    // range 4, filter 0: output = nibble * 16 (hand computed)
    const int a[16] = {1, 2, 3, 4, 5, 6, 7, -8, -1, -2, -3, -4, -5, -6, -7, 0};
    packBlock(blk, 0x40, a);
    assert(blk[1] == 0x12 && blk[4] == 0x78 && blk[5] == 0xFE && blk[8] == 0x90);
    assert(brrDecodeBlock(blk, st, out) == 0);
    const int16_t expA[16] = {16, 32, 48, 64, 80, 96, 112, -128, -16, -32, -48, -64, -80, -96, -112, 0};
    for (int i = 0; i < 16; ++i) assert(out[i] == expA[i]);

    // filter 1 / 2 / 3 against independently computed sequences (first four of F1 verified by hand: 64,124,182,236)
    const int n1[16] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    const int n2[16] = {3, -2, 5, -4, 1, 0, -7, 6, 2, -3, 4, -1, 7, -8, 0, 1};
    const int n3[16] = {2, 2, -3, -3, 5, 5, -6, -6, 1, 1, -2, -2, 7, 7, -8, -8};
    const int16_t e1[16] = {64, 124, 182, 236, 286, 334, 378, 420, 458, 494, 528, 560, 590, 618, 644, 668};
    const int16_t e2[16] = {24, 30, 74, 80, 92, 100, 48, 46, 58, 44, 60, 66, 124, 112, 96, 86};
    const int16_t e3[16] = {64, 180, 176, 74, 150, 368, 348, 134, -10, -94, -224, -390, -294, 12, 4, -258};
    struct { int hdr; const int* n; const int16_t* e; } cases[3] = {{(4 << 4) | (1 << 2), n1, e1}, {(3 << 4) | (2 << 2), n2, e2}, {(5 << 4) | (3 << 2), n3, e3}};
    for (auto& c : cases) {
        st = BrrFilterState{};
        packBlock(blk, c.hdr, c.n);
        brrDecodeBlock(blk, st, out);
        for (int i = 0; i < 16; ++i) assert(out[i] == c.e[i]);
    }
    // ranges above 12 collapse to -2048/0 (doubled on output); range 12 saturates through the 15-bit wrap
    BrrFilterState s0;
    assert(brrDecodeNibble(-1, 13, 0, s0) == -4096 && brrDecodeNibble(3, 13, 0, s0) == 0);
    s0 = BrrFilterState{};
    assert(brrDecodeNibble(7, 12, 0, s0) == 28672 && brrDecodeNibble(-8, 12, 0, s0) == -32768);

    // flags + multi-block: filter state carries across blocks, end flag stops decoding
    uint8_t two[27];
    const int ones[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    packBlock(two, (4 << 4) | (1 << 2), ones);
    packBlock(two + 9, (4 << 4) | (1 << 2) | 1 | 2, ones);  // end + loop
    packBlock(two + 18, 0x40, ones);                         // must never be reached
    int16_t pcm[48];
    const BrrDecodeInfo info = brrDecode(two, 3, pcm, 48);
    assert(info.blocks == 2 && info.samples == 32 && info.endFlag && info.loopFlag);
    BrrFilterState chk; int16_t b0[16], b1[16];
    brrDecodeBlock(two, chk, b0); brrDecodeBlock(two + 9, chk, b1);
    assert(std::memcmp(pcm, b0, sizeof b0) == 0 && std::memcmp(pcm + 16, b1, sizeof b1) == 0);
    assert(brrDecode(two, 3, pcm, 20).samples == 16);  // capacity respected
    std::puts("ok: BRR decode (all 4 filters, ranges, flags)");
}

static void testBrrEncode() {
    std::vector<int16_t> src(16 * 40);
    for (size_t i = 0; i < src.size(); ++i) src[i] = int16_t(12000 * std::sin(2 * 3.14159265 * double(i) / 37.0) + 3000 * std::sin(double(i) * 0.9));
    std::vector<uint8_t> bytes(brrEncodedSize(src.size()));
    assert(bytes.size() == 40 * 9);
    BrrEncodeOptions o; o.loop = true; o.loopStartSample = 16 * 8;
    assert(brrEncode(src.data(), src.size(), o, bytes.data(), bytes.size()) == bytes.size());
    assert(brrEncode(src.data(), src.size(), o, bytes.data(), bytes.size() - 1) == 0);
    std::vector<int16_t> dec(src.size());
    const BrrDecodeInfo info = brrDecode(bytes.data(), 40, dec.data(), dec.size());
    assert(info.samples == src.size() && info.endFlag && info.loopFlag);
    double sig = 0, err = 0;
    for (size_t i = 0; i < src.size(); ++i) { sig += double(src[i]) * src[i]; err += double(src[i] - dec[i]) * (src[i] - dec[i]); }
    assert(10 * std::log10(sig / err) > 30.0);
    assert(((bytes[0] >> 2) & 3) == 0 && ((bytes[8 * 9] >> 2) & 3) == 0);  // first + loop-entry blocks use filter 0
    assert((bytes[39 * 9] & 3) == 3 && (bytes[38 * 9] & 3) == 0);          // flags only on the last block
    o.loop = false;
    brrEncode(src.data(), src.size(), o, bytes.data(), bytes.size());
    assert((bytes[39 * 9] & 3) == 1);
    std::vector<int16_t> zero(16, 0);
    uint8_t zb[9];
    brrEncode(zero.data(), 16, BrrEncodeOptions{}, zb, 9);
    int16_t zo[16]; brrDecode(zb, 1, zo, 16);
    for (int16_t v : zo) assert(v == 0);
    std::puts("ok: BRR encode (SNR > 30 dB, filter-0 loop entry, flags)");
}

static void testGauss() {
    int16_t t[512];
    snesBuildGaussTable(t);
    int tapsA[4], tapsB[4];
    for (int i = 0; i < 256; ++i) {
        snesGaussTaps(t, i, tapsA);
        snesGaussTaps(t, 255 - i, tapsB);
        assert(tapsA[0] == tapsB[3] && tapsA[3] == tapsB[0] && tapsA[1] == tapsB[2] && tapsA[2] == tapsB[1]);  // mirror symmetry
        const int sum = tapsA[0] + tapsA[1] + tapsA[2] + tapsA[3];
        assert(sum >= 2040 && sum <= 2056);  // unity DC gain (2048) within rounding
        for (int k = 0; k < 4; ++k) assert(tapsA[k] >= 0);
    }
    snesGaussTaps(t, 0, tapsA);
    assert(tapsA[1] > 1250 && tapsA[1] < 1360);  // central tap near the silicon's ~1305 peak
    assert(tapsA[1] > tapsA[0] && tapsA[1] > tapsA[2] && tapsA[2] > tapsA[3]);
    for (int j = 0; j < 255; ++j) assert(t[j] <= t[j + 1] + 0 || j >= 256);  // table rises toward the centre
    std::puts("ok: Gaussian kernel symmetry, unity gain, shape");
}

static void testEcho() {
    SnesEcho e; e.init(44100.0);
    e.setDelayMs(10.f); e.setFeedback(0.f); e.setWetLevel(1.f);
    const int8_t fir[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    e.setFirHardware(fir);
    const size_t D = e.delaySamples();
    assert(D == 441);
    std::vector<float> L, R;
    for (size_t n = 0; n < D + 20; ++n) { float wl, wr; e.process(n == 0 ? 1.f : 0.f, n == 0 ? 0.5f : 0.f, wl, wr); L.push_back(wl); R.push_back(wr); }
    for (size_t n = 0; n < L.size(); ++n) {  // out[D+k] = tap[7-k] / 128
        const bool in = n >= D && n < D + 8;
        const float want = in ? float(fir[7 - (n - D)]) / 128.f : 0.f;
        assert(near(L[n], want) && near(R[n], want * 0.5f));
    }
    // feedback: identity FIR, fb 0.5 -> 1, 0.5, 0.25 at multiples of D
    SnesEcho f; f.init(44100.0); f.setDelayMs(10.f); f.setFeedback(0.5f); f.setWetLevel(1.f);
    const float id[8] = {0, 0, 0, 0, 0, 0, 0, 1};
    f.setFir(id);
    std::vector<float> o;
    for (size_t n = 0; n < 3 * D + 2; ++n) { float wl, wr; f.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); o.push_back(wl); }
    assert(near(o[D], 1.f) && near(o[2 * D], 0.5f) && near(o[3 * D], 0.25f) && o[D + 1] == 0.f);
    // wet level scales output; buffer-write off records nothing
    SnesEcho g; g.init(44100.0); g.setDelayMs(10.f); g.setFir(id); g.setWetLevel(0.25f);
    std::vector<float> w;
    for (size_t n = 0; n < D + 2; ++n) { float wl, wr; g.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); w.push_back(wl); }
    assert(near(w[D], 0.25f));
    SnesEcho h; h.init(44100.0); h.setDelayMs(10.f); h.setFir(id); h.setWetLevel(1.f); h.setBufferWrite(false);
    for (size_t n = 0; n < D + 2; ++n) { float wl, wr; h.process(n == 0 ? 1.f : 0.f, 0.f, wl, wr); assert(wl == 0.f); }
    std::puts("ok: echo FIR impulse response, feedback, wet level, write-disable");
}

static SnesPcmCore* makeCore(double sr) {
    auto* c = new SnesPcmCore();
    c->init(sr, 1 << 18);
    assert(c->loadDemoSamples() == 0 && c->sampleSlots() == kSnesDemoSamples);
    c->setGlobalParam(SnesParam::MasterVolume, 100);
    c->setGlobalParam(SnesParam::EchoWet, 0);
    return c;
}

static void testEnvelope() {
    SnesPcmCore* c = makeCore(32000.0);  // one envelope tick per rendered sample
    float l, r;
    auto tick = [&](int n) { for (int i = 0; i < n; ++i) c->render(&l, &r, 1); };
    // AR=0: first step lands on tick 2048 (+32); attack ends at 0x7E0 after 63 steps
    c->setChannelParam(0, SnesParam::Attack, 0);
    c->noteOn(0, 60, 100);
    tick(2047); assert(c->voiceEnvelope(0) == 0);
    tick(1);    assert(c->voiceEnvelope(0) == 32);
    tick(2048 * 62); assert(c->voiceEnvelope(0) >= 0x7E0 - 1 && c->voiceEnvelope(0) <= 0x7FF);
    c->allSoundsOff();

    // AR=15, DR=7, SL=7, SR=0: instant attack, one decay step, hold at 2039
    c->setChannelParam(0, SnesParam::Attack, 15);
    c->noteOn(0, 60, 100);
    tick(100); assert(c->voiceEnvelope(0) == 2039);
    tick(32000); assert(c->voiceEnvelope(0) == 2039);
    c->noteOff(0, 60);
    tick(100); assert(c->voiceEnvelope(0) == 2039 - 800);  // release: -8 per tick
    tick(160); assert(c->activeVoices() == 0);              // 2039/8 = 255 ticks total
    // sustain level 3 settles with (env >> 8) == 3 and then holds (SR = 0)
    c->setChannelParam(0, SnesParam::SustainLevel, 3);
    c->noteOn(0, 60, 100);
    tick(4000); const int held = c->voiceEnvelope(0);
    assert((held >> 8) == 3);
    tick(1000); assert(c->voiceEnvelope(0) == held);
    c->allSoundsOff();

    // GAIN: direct, linear increase, bent line
    c->setChannelParam(0, SnesParam::SustainLevel, 7);
    c->setChannelParam(0, SnesParam::UseGain, 1);
    c->setChannelParam(0, SnesParam::GainValue, 0x40);
    c->noteOn(0, 60, 100); assert(c->voiceEnvelope(0) == 0x40 * 16); c->allSoundsOff();
    c->setChannelParam(0, SnesParam::GainValue, 0x80 | (2 << 5) | 31);
    c->noteOn(0, 60, 100); tick(10); assert(c->voiceEnvelope(0) == 320); c->allSoundsOff();
    c->setChannelParam(0, SnesParam::GainValue, 0x80 | (3 << 5) | 31);
    c->noteOn(0, 60, 100);
    tick(48); assert(c->voiceEnvelope(0) == 0x600);
    tick(4);  assert(c->voiceEnvelope(0) == 0x600 + 4 * 8);  // slope drops to +8 above 0x600
    delete c;
    std::puts("ok: ADSR tick-exact timing, sustain level, GAIN modes, release");
}

static std::vector<float> renderSecond(SnesPcmCore& c, size_t frames = 44100) {
    std::vector<float> l(frames), r(frames);
    c.render(l.data(), r.data(), frames);
    return l;
}
static int risingCrossings(const std::vector<float>& x) {
    int n = 0; for (size_t i = 1; i < x.size(); ++i) if (x[i - 1] < 0 && x[i] >= 0) ++n;
    return n;
}

static void testPitchVoicesRouting() {
    SnesPcmCore* c = makeCore(44100.0);
    c->noteOn(0, 60, 127);
    int z = risingCrossings(renderSecond(*c));
    assert(std::abs(z - 262) <= 3);
    c->allSoundsOff(); c->noteOn(0, 72, 127);
    z = risingCrossings(renderSecond(*c)); assert(std::abs(z - 523) <= 5);
    c->allSoundsOff(); c->pitchBend(0, 16383); c->noteOn(0, 60, 127);
    z = risingCrossings(renderSecond(*c)); assert(std::abs(z - 294) <= 4);  // +2 semitones
    c->allSoundsOff(); c->pitchBend(0, 8192);

    // voice limit: authentic 8 by default, raisable, clamped to the compile-time maximum
    for (int n = 0; n < 12; ++n) c->noteOn(0, 40 + n, 100);
    assert(c->activeVoices() == 8);
    c->setGlobalParam(SnesParam::VoiceLimit, 12);
    for (int n = 0; n < 12; ++n) c->noteOn(0, 50 + n, 100);
    assert(c->activeVoices() == 12);
    c->setGlobalParam(SnesParam::VoiceLimit, 999);
    assert(c->globalParam(SnesParam::VoiceLimit) == kSnesMaxVoices);
    c->setGlobalParam(SnesParam::VoiceLimit, 8);
    assert(c->activeVoices() <= 8);
    c->allSoundsOff();

    // sustain pedal holds a released note until the pedal lifts
    c->controlChange(0, 64, 127); c->noteOn(0, 60, 100); c->noteOff(0, 60);
    renderSecond(*c, 4410); assert(c->activeVoices() == 1);
    c->controlChange(0, 64, 0); renderSecond(*c, 4410); assert(c->activeVoices() == 0);

    // brightness (CC74) lowers high-frequency content of the pulse sample
    auto roughness = [&](int bright) {
        c->allSoundsOff(); c->setChannelParam(1, SnesParam::Slot, 1);
        c->controlChange(1, 74, bright); c->noteOn(1, 60, 127);
        auto x = renderSecond(*c, 8820); double s = 0;
        for (size_t i = 4410; i < x.size(); ++i) s += std::fabs(x[i] - x[i - 1]);
        return s;
    };
    assert(roughness(30) < roughness(127) * 0.7);

    // wet-only echo bus: silent until the echo delay, only fed by echo-send channels, dry untouched
    auto split = [&](int echoSend, int wetLevel, std::vector<float>& dry, std::vector<float>& wet) {
        c->allSoundsOff(); c->reset(); c->setGlobalParam(SnesParam::MasterVolume, 100);
        c->setGlobalParam(SnesParam::EchoDelayMs, 20); c->setGlobalParam(SnesParam::EchoWet, wetLevel);
        c->setChannelParam(2, SnesParam::EchoSend, echoSend); c->setChannelParam(2, SnesParam::Slot, 2);
        std::vector<float> dr(4410), wr(4410), a(4410), b(4410);
        c->noteOn(2, 60, 127);
        c->renderSplit(a.data(), dr.data(), b.data(), wr.data(), 4410);
        dry = a; wet = b;
    };
    std::vector<float> dry1, wet1, dry2, wet2, dry3, wet3;
    split(1, 100, dry1, wet1); split(1, 0, dry2, wet2); split(0, 100, dry3, wet3);
    const size_t D = 882;
    for (size_t n = 0; n < D; ++n) assert(wet1[n] == 0.f);
    double e = 0; for (size_t n = D; n < wet1.size(); ++n) e += std::fabs(wet1[n]);
    assert(e > 0.1);
    for (float v : wet2) assert(v == 0.f);   // wet level 0
    for (float v : wet3) assert(v == 0.f);   // channel not sent to echo
    assert(dry1 == dry2 && dry1 == dry3);    // dry bus independent of echo settings
    delete c;
    std::puts("ok: pitch, voice limit (8, raisable), sustain pedal, CC74, wet-only echo routing");
}

static void testFxIntegration() {
    auto run = [](int fxEnable, int cc91) {
        SnesPcmCore* c = makeCore(44100.0);
        c->fx().setParam(FxParam::Enable, fxEnable);
        c->fx().setParam(FxParam::ReverbLevel, 100);
        c->controlChange(0, 91, cc91);
        c->noteOn(0, 60, 127);
        auto a = renderSecond(*c, 6000);
        c->noteOff(0, 60);
        auto b = renderSecond(*c, 40000);
        a.insert(a.end(), b.begin(), b.end());
        delete c;
        return a;
    };
    const auto off = run(0, 0), onZero = run(1, 0), onSend = run(1, 127);
    assert(off == onZero);   // enabled with zero send = bit-exact passthrough
    assert(off != onSend);
    double tailOff = 0, tailOn = 0;
    for (size_t i = 30000; i < off.size(); ++i) { tailOff += double(off[i]) * off[i]; tailOn += double(onSend[i]) * onSend[i]; }
    assert(tailOn > tailOff * 5.0);  // reverb tail from CC91
    std::puts("ok: FX rack integration (CC91 send, exact bypass)");
}

static void setupScene(SnesPcmCore& c) {
    c.setGlobalParam(SnesParam::EchoWet, 50);
    c.fx().setParam(FxParam::Enable, 1);
    for (int ch = 0; ch < 3; ++ch) {
        c.setChannelParam(ch, SnesParam::Slot, ch);
        c.setChannelParam(ch, SnesParam::EchoSend, 1);
        c.setChannelParam(ch, SnesParam::Pan, 20 + ch * 40);
        c.controlChange(ch, 91, 50); c.controlChange(ch, 93, 40); c.controlChange(ch, 94, 30);
        c.controlChange(ch, 1, 40);
        c.noteOn(ch, 55 + ch * 7, 100);
    }
}

static void testDeterminismStateAlloc() {
    SnesPcmCore* a = makeCore(44100.0); setupScene(*a);
    SnesPcmCore* b = makeCore(44100.0); setupScene(*b);
    assert(fnv(renderSecond(*a)) == fnv(renderSecond(*b)));
    delete a; delete b;

    SnesPcmCore* c = makeCore(44100.0); setupScene(*c);
    std::vector<float> l(512), r(512);
    const size_t n0 = g_allocs;
    for (int i = 0; i < 60; ++i) {
        c->render(l.data(), r.data(), 512);
        c->noteOn(i % 3, 48 + i % 24, 80 + i % 40); c->noteOff((i + 1) % 3, 55);
        c->controlChange(i % 3, 74, i * 2); c->controlChange(i % 3, 71, i); c->pitchBend(i % 3, 8192 + i * 50);
        c->setChannelParam(i % 3, SnesParam::Attack, i % 16); c->setGlobalParam(SnesParam::EchoDelayMs, 20 + i);
    }
    assert(g_allocs == n0);
    delete c;

    SnesPcmCore* s = makeCore(44100.0); setupScene(*s); renderSecond(*s, 7000);
    std::vector<uint8_t> blob(s->stateSize());
    size_t written = 0;
    assert(s->saveState(blob.data(), blob.size(), &written) && written == blob.size());
    const uint64_t cont = fnv(renderSecond(*s, 20000));
    SnesPcmCore* t = makeCore(44100.0);
    assert(t->loadState(blob.data(), blob.size()));
    assert(fnv(renderSecond(*t, 20000)) == cont);
    assert(!t->loadState(blob.data(), blob.size() - 1));
    SnesPcmCore* bare = new SnesPcmCore(); bare->init(44100.0, 1 << 18);  // no samples loaded -> mismatch rejected
    assert(!bare->loadState(blob.data(), blob.size()));
    delete s; delete t; delete bare;
    std::puts("ok: determinism, zero allocation, state round-trip");
}

static void testParams() {
    SnesPcmCore c; c.init(44100.0, 1 << 16);
    for (size_t i = 0; i < kSnesParamTable.size(); ++i) {
        const auto& t = kSnesParamTable[i];
        assert(t.name && t.min <= t.def && t.def <= t.max);
        const SnesParam p = SnesParam(i);
        if (t.scope == SnesParamScope::Global) {
            assert(c.globalParam(p) == t.def);
            c.setGlobalParam(p, 100000); assert(c.globalParam(p) == t.max);
            c.setGlobalParam(p, -100000); assert(c.globalParam(p) == t.min);
        } else {
            assert(c.channelParam(3, p) == t.def);
            c.setChannelParam(3, p, 100000); assert(c.channelParam(3, p) == t.max);
            c.setChannelParam(3, p, -100000); assert(c.channelParam(3, p) == t.min);
        }
    }
    assert(!c.setGlobalParam(SnesParam::Pan, 1) && !c.setChannelParam(0, SnesParam::EchoWet, 1) && !c.setChannelParam(16, SnesParam::Pan, 1));
    std::puts("ok: parameter table, clamping, scope checks");
}

int main() {
    testBrrDecode(); testBrrEncode(); testGauss(); testEcho(); testEnvelope();
    testPitchVoicesRouting(); testFxIntegration(); testDeterminismStateAlloc(); testParams();
    std::puts("SnesPcmCore: ALL PASSED");
    return 0;
}
