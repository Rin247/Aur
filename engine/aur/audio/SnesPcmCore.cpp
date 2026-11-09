#include "SnesPcmCore.hpp"

#include <algorithm>
#include <cmath>

#include "BrrCodec.hpp"

namespace aur::audio {
namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr double kHwRate = 32000.0;  // chip envelope/counter clock domain
constexpr float kBendSemis = 2.0f;
constexpr uint32_t kStateMagic = 0x53504331;  // "SPC1"
// Samples-per-step for each of the 32 envelope rates (0 = never).
constexpr int kPeriod[32] = {0, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
                             64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

void snesBuildGaussTable(int16_t out[512], double sigma) {
    double w[512];
    for (int j = 0; j < 512; ++j) {
        const double d = 2.0 - (j + 0.5) / 256.0;  // distance of tap j from the read position, in samples
        w[j] = std::exp(-d * d / (2.0 * sigma * sigma));
    }
    double mean = 0;
    for (int i = 0; i < 256; ++i) mean += w[255 - i] + w[511 - i] + w[256 + i] + w[i];
    const double scale = 2048.0 / (mean / 256.0);
    for (int j = 0; j < 512; ++j) out[j] = int16_t(std::lround(w[j] * scale));
}

void snesGaussTaps(const int16_t t[512], int i, int out[4]) {
    out[0] = t[255 - i]; out[1] = t[511 - i]; out[2] = t[256 + i]; out[3] = t[i];
}

// ---------------------------------------------------------------- lifecycle
void SnesPcmCore::init(double sampleRate, size_t poolFrames) {
    sr_ = sampleRate;
    pool_.assign(poolFrames, 0);
    snesBuildGaussTable(gauss_.data());
    echo_.init(sampleRate);
    fx_.init(sampleRate);
    clearSamples();
    reset();
}

void SnesPcmCore::reset() {
    for (size_t i = 0; i < g_.size(); ++i) g_[i] = kSnesParamTable[i].def;
    for (auto& s : setup_) s = SnesChannelSetup{};
    for (auto& c : chan_) c = Chan{};
    for (auto& v : voices_) v = Voice{};
    ageCounter_ = 0; egCounter_ = 0; egAcc_ = 0;
    echo_.reset(); fx_.reset();
    echo_.setDelayMs(float(g_[2])); echo_.setFeedback(float(g_[3]) / 100.f);
    echo_.setWetLevel(float(g_[4]) / 100.f); echo_.setBufferWrite(g_[5] != 0);
    int8_t t[8];
    for (int k = 0; k < 8; ++k) t[k] = int8_t(g_[size_t(6 + k)]);
    echo_.setFirHardware(t);
    for (int c = 0; c < kSnesChannels; ++c) recompute(c);
}

void SnesPcmCore::recompute(int ch) {
    Chan& c = chan_[size_t(ch)];
    const SnesChannelSetup& s = setup_[size_t(ch)];
    const float vol = float(s.volume) / 127.f, ex = float(c.expr) / 127.f;
    c.gain = vol * vol * ex * ex;
    const double a = double(s.pan) / 127.0 * kTwoPi / 4.0;
    c.gl = float(std::cos(a) * 1.41421356); c.gr = float(std::sin(a) * 1.41421356);
    const double fc = std::min(80.0 * std::exp2(double(c.bright) / 127.0 * 8.0), 0.45 * sr_);
    const double g = std::tan(3.14159265358979 * fc / sr_), k = 2.0 * (1.0 - 0.9 * double(c.res) / 127.0);
    const double a1 = 1.0 / (1.0 + g * (g + k));
    c.k = float(k); c.a1 = float(a1); c.a2 = float(g * a1); c.a3 = float(g * g * a1);
}

// --------------------------------------------------------------- parameters
bool SnesPcmCore::setGlobalParam(SnesParam p, int v) {
    const auto& info = kSnesParamTable[size_t(p)];
    if (info.scope != SnesParamScope::Global) return false;
    v = clampi(v, info.min, info.max);
    g_[size_t(p)] = v;
    if (p == SnesParam::VoiceLimit) { for (int i = v; i < kSnesMaxVoices; ++i) voices_[size_t(i)] = Voice{}; }
    else if (p == SnesParam::EchoDelayMs) echo_.setDelayMs(float(v));
    else if (p == SnesParam::EchoFeedback) echo_.setFeedback(float(v) / 100.f);
    else if (p == SnesParam::EchoWet) echo_.setWetLevel(float(v) / 100.f);
    else if (p == SnesParam::EchoWriteEnable) echo_.setBufferWrite(v != 0);
    else if (size_t(p) >= 6) {
        int8_t t[8];
        for (int k = 0; k < 8; ++k) t[k] = int8_t(g_[size_t(6 + k)]);
        echo_.setFirHardware(t);
    }
    return true;
}

bool SnesPcmCore::setChannelParam(int ch, SnesParam p, int v) {
    if (ch < 0 || ch >= kSnesChannels) return false;
    const auto& info = kSnesParamTable[size_t(p)];
    if (info.scope != SnesParamScope::Channel) return false;
    v = clampi(v, info.min, info.max);
    SnesChannelSetup& s = setup_[size_t(ch)];
    switch (p) {
    case SnesParam::Slot: s.slot = v; break;
    case SnesParam::Attack: s.attack = v; break;
    case SnesParam::Decay: s.decay = v; break;
    case SnesParam::SustainLevel: s.sustainLevel = v; break;
    case SnesParam::SustainRate: s.sustainRate = v; break;
    case SnesParam::UseGain: s.useGain = v; break;
    case SnesParam::GainValue: s.gain = v; break;
    case SnesParam::Pan: s.pan = v; break;
    case SnesParam::Volume: s.volume = v; break;
    case SnesParam::EchoSend: s.echoSend = v; break;
    default: s.tuneCents = v; break;
    }
    recompute(ch);
    return true;
}

int SnesPcmCore::channelParam(int ch, SnesParam p) const {
    const SnesChannelSetup& s = setup_[size_t(ch)];
    switch (p) {
    case SnesParam::Slot: return s.slot;
    case SnesParam::Attack: return s.attack;
    case SnesParam::Decay: return s.decay;
    case SnesParam::SustainLevel: return s.sustainLevel;
    case SnesParam::SustainRate: return s.sustainRate;
    case SnesParam::UseGain: return s.useGain;
    case SnesParam::GainValue: return s.gain;
    case SnesParam::Pan: return s.pan;
    case SnesParam::Volume: return s.volume;
    case SnesParam::EchoSend: return s.echoSend;
    default: return s.tuneCents;
    }
}

void SnesPcmCore::setChannelSetup(int ch, const SnesChannelSetup& in) {
    if (ch < 0 || ch >= kSnesChannels) return;
    SnesChannelSetup& s = setup_[size_t(ch)];
    s.slot = clampi(in.slot, 0, kSnesMaxSlots - 1);
    s.attack = clampi(in.attack, 0, 15); s.decay = clampi(in.decay, 0, 7);
    s.sustainLevel = clampi(in.sustainLevel, 0, 7); s.sustainRate = clampi(in.sustainRate, 0, 31);
    s.useGain = clampi(in.useGain, 0, 1); s.gain = clampi(in.gain, 0, 255);
    s.pan = clampi(in.pan, 0, 127); s.volume = clampi(in.volume, 0, 127);
    s.echoSend = clampi(in.echoSend, 0, 1); s.tuneCents = clampi(in.tuneCents, -100, 100);
    recompute(ch);
}

// --------------------------------------------------------------------- MIDI
void SnesPcmCore::noteOn(int ch, int note, int vel) {
    if (ch < 0 || ch >= kSnesChannels) return;
    if (vel <= 0) { noteOff(ch, note); return; }
    const SnesChannelSetup& s = setup_[size_t(ch)];
    if (s.slot >= nSlots_) return;
    const int limit = g_[0];
    int idx = -1;
    for (int i = 0; i < limit; ++i) if (!voices_[size_t(i)].active) { idx = i; break; }
    if (idx < 0) {  // steal a released voice first, then the oldest
        uint64_t best = ~0ull; bool bestRel = false;
        for (int i = 0; i < limit; ++i) {
            const Voice& v = voices_[size_t(i)];
            if ((v.released && !bestRel) || (v.released == bestRel && v.age < best)) { idx = i; best = v.age; bestRel = v.released; }
        }
    }
    Voice& v = voices_[size_t(idx)];
    v = Voice{};
    v.active = true; v.ch = ch; v.note = clampi(note, 0, 127); v.vel = clampi(vel, 1, 127); v.slot = s.slot;
    v.ar = s.attack; v.dr = s.decay; v.sl = s.sustainLevel; v.sr = s.sustainRate;
    v.useGain = s.useGain; v.gain = s.gain; v.echo = s.echoSend; v.tune = s.tuneCents;
    v.age = ++ageCounter_;
    v.stage = s.useGain ? kGain : kAttack;
    v.env = (s.useGain && !(s.gain & 0x80)) ? (s.gain & 0x7F) * 16 : 0;
}

void SnesPcmCore::noteOff(int ch, int note) {
    if (ch < 0 || ch >= kSnesChannels) return;
    for (int i = 0; i < g_[0]; ++i) {
        Voice& v = voices_[size_t(i)];
        if (!v.active || v.released || v.ch != ch || v.note != note) continue;
        if (chan_[size_t(ch)].sustain) v.sustainHeld = true; else release(v);
    }
}

void SnesPcmCore::pitchBend(int ch, int v14) {
    if (ch < 0 || ch >= kSnesChannels) return;
    chan_[size_t(ch)].bend = float(clampi(v14, 0, 16383) - 8192) / 8192.f * kBendSemis;
}

void SnesPcmCore::controlChange(int ch, int cc, int val) {
    if (ch < 0 || ch >= kSnesChannels) return;
    Chan& c = chan_[size_t(ch)];
    val = clampi(val, 0, 127);
    switch (cc) {
    case 1: c.mod = val; break;
    case 7: setup_[size_t(ch)].volume = val; recompute(ch); break;
    case 10: setup_[size_t(ch)].pan = val; recompute(ch); break;
    case 11: c.expr = val; recompute(ch); break;
    case 64:
        c.sustain = val >= 64;
        if (!c.sustain)
            for (auto& v : voices_) if (v.active && v.ch == ch && v.sustainHeld && !v.released) release(v);
        break;
    case 71: c.res = val; recompute(ch); break;
    case 74: c.bright = val; recompute(ch); break;
    case 91: c.rev = float(val) / 127.f; break;
    case 93: c.cho = float(val) / 127.f; break;
    case 94: c.dly = float(val) / 127.f; break;
    case 120: for (auto& v : voices_) if (v.ch == ch) v = Voice{}; break;
    case 123: for (auto& v : voices_) if (v.active && v.ch == ch && !v.released) release(v); break;
    default: break;
    }
}

void SnesPcmCore::allNotesOff() { for (auto& v : voices_) if (v.active && !v.released) release(v); }
void SnesPcmCore::allSoundsOff() { for (auto& v : voices_) v = Voice{}; }
int SnesPcmCore::activeVoices() const { int n = 0; for (const auto& v : voices_) n += v.active ? 1 : 0; return n; }

// ----------------------------------------------------------------- envelope
void SnesPcmCore::tickEnv(Voice& v) {
    int rate = 0;
    switch (v.stage) {
    case kAttack: rate = v.ar * 2 + 1; break;
    case kDecay: rate = v.dr * 2 + 16; break;
    case kSustain: rate = v.sr; break;
    case kRelease: rate = 31; break;
    case kGain:
        if (!(v.gain & 0x80)) { v.env = (v.gain & 0x7F) * 16; return; }  // direct level
        rate = v.gain & 31;
        break;
    default: return;
    }
    const int period = kPeriod[rate];
    if (period == 0 || egCounter_ % uint32_t(period) != 0) return;
    switch (v.stage) {
    case kAttack:
        v.env += rate == 31 ? 1024 : 32;
        if (v.env >= 0x7E0) { v.env = std::min(v.env, 0x7FF); v.stage = kDecay; }
        break;
    case kDecay:
        v.env -= ((v.env - 1) >> 8) + 1;
        if (v.env < 0) v.env = 0;
        if ((v.env >> 8) <= v.sl) v.stage = kSustain;
        break;
    case kSustain:
        v.env -= ((v.env - 1) >> 8) + 1;
        if (v.env <= 0) { v.env = 0; v.active = false; v.stage = kOff; }
        break;
    case kRelease:
        v.env -= 8;
        if (v.env <= 0) { v.env = 0; v.active = false; v.stage = kOff; }
        break;
    default: {  // GAIN: 0 linear dec, 1 exponential dec, 2 linear inc, 3 bent-line inc
        const int mode = (v.gain >> 5) & 3;
        if (mode == 0) v.env -= 32;
        else if (mode == 1) v.env -= ((v.env - 1) >> 8) + 1;
        else if (mode == 2) v.env += 32;
        else v.env += v.env < 0x600 ? 32 : 8;
        v.env = clampi(v.env, 0, 0x7FF);
        break;
    }
    }
}

// ------------------------------------------------------------------- render
float SnesPcmCore::interp(const Slot& s, double pos) const {
    const long i = long(pos);
    int t[4];
    snesGaussTaps(gauss_.data(), std::min(255, int((pos - double(i)) * 256.0)), t);
    float acc = 0;
    for (int k = 0; k < 4; ++k) {
        long idx = i - 1 + k;
        if (s.loop && idx >= long(s.len) && s.len > s.loopStart) idx = long(s.loopStart) + (idx - long(s.len)) % long(s.len - s.loopStart);
        if (idx >= 0 && idx < long(s.len)) acc += float(t[k]) * float(pool_[s.off + size_t(idx)]);
    }
    return acc * (1.f / 2048.f);
}

void SnesPcmCore::renderFrame(float& dL, float& dR, float& wL, float& wR, Bus& bus) {
    const int limit = g_[0];
    egAcc_ += kHwRate / sr_;
    while (egAcc_ >= 1.0) {
        egAcc_ -= 1.0; ++egCounter_;
        for (int i = 0; i < limit; ++i) if (voices_[size_t(i)].active) tickEnv(voices_[size_t(i)]);
    }
    float eL = 0, eR = 0;
    dL = dR = 0;
    for (int i = 0; i < limit; ++i) {
        Voice& v = voices_[size_t(i)];
        if (!v.active) continue;
        const Slot& s = slots_[size_t(v.slot)];
        Chan& c = chan_[size_t(v.ch)];
        if ((v.counter++ & 7) == 0) {  // pitch (bend, tune, vibrato) refreshed every 8 samples
            v.vib += float(5.5 * 8.0 / sr_);
            v.vib -= std::floor(v.vib);
            const double cents = double(v.tune) + double(c.mod) / 127.0 * 50.0 * std::sin(kTwoPi * double(v.vib));
            v.step = s.rate / sr_ * std::exp2((double(v.note - s.root) + double(c.bend) + cents / 100.0) / 12.0);
        }
        const float vg = float(v.vel) / 127.f;
        float y = interp(s, v.pos) * (1.f / 32768.f) * (float(v.env) / 2048.f) * vg * vg * c.gain;
        if (c.bright < 127 || c.res > 0) {  // TPT state-variable lowpass (modern extension, CC74/CC71)
            const float v3 = y - v.ic2;
            const float v1 = c.a1 * v.ic1 + c.a2 * v3;
            const float v2 = v.ic2 + c.a2 * v.ic1 + c.a3 * v3;
            v.ic1 = 2.f * v1 - v.ic1; v.ic2 = 2.f * v2 - v.ic2;
            y = v2;
        }
        const float yl = y * c.gl, yr = y * c.gr;
        dL += yl; dR += yr;
        if (v.echo) { eL += yl; eR += yr; }
        bus.rvL += yl * c.rev; bus.rvR += yr * c.rev;
        bus.chL += yl * c.cho; bus.chR += yr * c.cho;
        bus.dlL += yl * c.dly; bus.dlR += yr * c.dly;
        v.pos += v.step;
        if (s.loop) { const double len = double(s.len - s.loopStart); while (v.pos >= double(s.len)) v.pos -= len; }
        else if (v.pos >= double(s.len)) { v.active = false; v.stage = kOff; }
    }
    echo_.process(eL, eR, wL, wR);
}

void SnesPcmCore::render(float* left, float* right, size_t frames) {
    const float m = float(g_[1]) / 100.f;
    for (size_t n = 0; n < frames; ++n) {
        float dL, dR, wL, wR, oL, oR;
        Bus b;
        renderFrame(dL, dR, wL, wR, b);
        fx_.processSample((dL + wL) * m, (dR + wR) * m, b.chL * m, b.chR * m, b.dlL * m, b.dlR * m, b.rvL * m, b.rvR * m, oL, oR);
        left[n] = std::clamp(oL, -1.f, 1.f);
        right[n] = std::clamp(oR, -1.f, 1.f);
    }
}

void SnesPcmCore::renderSplit(float* dryL, float* dryR, float* wetL, float* wetR, size_t frames) {
    const float m = float(g_[1]) / 100.f;
    for (size_t n = 0; n < frames; ++n) {
        float dL, dR, wL, wR;
        Bus b;
        renderFrame(dL, dR, wL, wR, b);
        dryL[n] = dL * m; dryR[n] = dR * m; wetL[n] = wL * m; wetR[n] = wR * m;
    }
}

// -------------------------------------------------------------------- state
template <class V> void SnesPcmCore::visit(V& v) {
    v(g_); v(setup_); v(chan_); v(voices_); v(slots_); v(nSlots_); v(poolUsed_);
    v(ageCounter_); v(egCounter_); v(egAcc_);
    echo_.visit(v); fx_.visit(v);
}
size_t SnesPcmCore::stateSize() { StateSizer s; visit(s); return s.size() + 3 * sizeof(uint32_t) + sizeof(double); }

bool SnesPcmCore::saveState(uint8_t* dst, size_t cap, size_t* written) {
    StateWriter w(dst, cap);
    uint32_t hdr[3] = {kStateMagic, uint32_t(poolUsed_), uint32_t(nSlots_)};
    double sr = sr_;
    w(hdr); w(sr); visit(w);
    if (written) *written = w.size();
    return w.ok();
}

bool SnesPcmCore::loadState(const uint8_t* src, size_t len) {
    if (len != stateSize()) return false;  // exact size required: nothing is modified on mismatch
    StateReader r(src, len);
    uint32_t hdr[3] = {};
    double sr = 0;
    r(hdr); r(sr);
    if (hdr[0] != kStateMagic || hdr[1] != uint32_t(poolUsed_) || hdr[2] != uint32_t(nSlots_) || sr != sr_) return false;
    visit(r);
    return r.ok();
}

}  // namespace aur::audio
