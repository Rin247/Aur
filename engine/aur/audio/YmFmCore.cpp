#include "YmFmCore.hpp"

#include <algorithm>
#include <cmath>

#include "YmFmPatches.hpp"

namespace aur::audio {
namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr double kEgRateHz = 53267.0 / 3.0;  // chip EG clock
constexpr float kModCycles = 4.0f;           // full-scale operator output = +-4 cycles of phase
constexpr float kBendSemis = 2.0f;
constexpr int kMaxAtt = 1023;                // 10-bit attenuation, 0.09375 dB/unit
constexpr float kLfoHz[8] = {3.98f, 5.56f, 6.02f, 6.37f, 6.88f, 9.63f, 48.1f, 72.2f};
constexpr float kPmCents[8] = {0, 3.4f, 6.7f, 10, 14, 20, 40, 80};
constexpr float kAmDb[4] = {0, 1.4f, 5.9f, 11.8f};
constexpr uint8_t kCarrier[8] = {8, 8, 8, 8, 10, 14, 14, 15};

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Average per-EG-tick increment for rate r is (4 + r%4) * 2^(r/4) / 2^14 attenuation units.
int egUnits(uint32_t& frac, int rate) {
    if (rate <= 0) return 0;
    frac += uint32_t(4 + (rate & 3)) << (rate >> 2);
    const int u = int(frac >> 14);
    frac &= 0x3FFF;
    return u;
}
int slLevel(int d1l) { return d1l >= 15 ? 0x3E0 : d1l << 5; }
}  // namespace

float ymEvalAlgorithm(int alg, const float amp[4], const float phase[4], float fbMod, float modScale, float* op1Out) {
    auto op = [&](int i, float mod) {
        return amp[i] * float(std::sin(kTwoPi * (double(phase[i]) + double(mod) * double(modScale))));
    };
    const float o1 = op(0, fbMod);
    if (op1Out) *op1Out = o1;
    float o2, o3, o4;
    switch (alg & 7) {
    case 0: o2 = op(1, o1); o3 = op(2, o2); o4 = op(3, o3); return o4;
    case 1: o2 = op(1, 0); o3 = op(2, o1 + o2); o4 = op(3, o3); return o4;
    case 2: o2 = op(1, 0); o3 = op(2, o2); o4 = op(3, o1 + o3); return o4;
    case 3: o2 = op(1, o1); o3 = op(2, 0); o4 = op(3, o2 + o3); return o4;
    case 4: o2 = op(1, o1); o3 = op(2, 0); o4 = op(3, o3); return o2 + o4;
    case 5: o2 = op(1, o1); o3 = op(2, o1); o4 = op(3, o1); return o2 + o3 + o4;
    case 6: o2 = op(1, o1); o3 = op(2, 0); o4 = op(3, 0); return o2 + o3 + o4;
    default: o2 = op(1, 0); o3 = op(2, 0); o4 = op(3, 0); return o1 + o2 + o3 + o4;
    }
}
int ymKeyCode(int note) { return clampi((note - 12) / 3, 0, 31); }
uint8_t ymCarrierMask(int alg) { return kCarrier[alg & 7]; }

void YmFmCore::init(double sampleRate) {
    sr_ = sampleRate;
    fx_.init(sampleRate);
    for (int i = 0; i < 1024; ++i)
        gainTab_[size_t(i)] = i >= kMaxAtt ? 0.f : float(std::pow(10.0, -double(i) * 0.09375 / 20.0));
    reset();
}

void YmFmCore::reset() {
    for (size_t i = 0; i < 8; ++i) g_[i] = kYmParamTable[i].def;
    for (auto& c : ch_) c = YmChannelParams{};
    for (auto& r : rt_) r = ChRt{};
    for (int c = 0; c < kYmChannels; ++c) recomputePan(c);
    lfoPhase_ = egAcc_ = 0;
    fx_.reset();
}

void YmFmCore::setDacBuffer(const uint8_t* data, size_t length, double baseRateHz) {
    dac_ = data; dacLen_ = length; dacRate_ = baseRateHz > 1.0 ? baseRateHz : 1.0;
    rt_[5].dacPlaying = false;
}

// -------------------------------------------------------------------- MIDI
void YmFmCore::updateIncs(int ch) {
    ChRt& c = rt_[size_t(ch)];
    const double f = 440.0 * std::exp2((double(c.note) + double(c.bend) - 69.0) / 12.0);
    for (int i = 0; i < kYmOps; ++i) {
        const YmOperatorParams& p = ch_[size_t(ch)].op[size_t(i)];
        const double mul = p.mul == 0 ? 0.5 : double(p.mul);
        const double cents = (p.dt & 4 ? -1.0 : 1.0) * double(p.dt & 3) * 2.5;
        c.op[size_t(i)].inc = f * mul * std::exp2(cents / 1200.0) / sr_;
    }
}

void YmFmCore::recomputePan(int ch) {
    ChRt& c = rt_[size_t(ch)];
    const int bits = ch_[size_t(ch)].pan;
    const bool l = bits & 2, r = bits & 1;
    if (gp(YmParam::SoftPan)) {
        const double a = double(c.ccPan) / 127.0 * kTwoPi / 4.0;
        c.gl = l ? float(std::cos(a) * 1.41421356) : 0.f;
        c.gr = r ? float(std::sin(a) * 1.41421356) : 0.f;
    } else {
        const bool cl = c.ccPan < 43, cr = c.ccPan > 85;  // CC10 -> hard L / centre / R
        c.gl = (l && !cr) ? 1.f : 0.f;
        c.gr = (r && !cl) ? 1.f : 0.f;
    }
}

void YmFmCore::keyOn(int ch) {
    ChRt& c = rt_[size_t(ch)];
    for (int i = 0; i < kYmOps; ++i) {
        OpRt& o = c.op[size_t(i)];
        const int ssg = ch_[size_t(ch)].op[size_t(i)].ssg;
        o.phase = 0; o.stage = kAttack; o.frac = o.attFrac = 0;
        o.inv = (ssg & 8) ? uint8_t((ssg >> 2) & 1) : uint8_t(0);
        o.held = o.ssgActive = 0;
    }
    c.fb1 = c.fb2 = 0;
    c.keyOn = true; c.pendingOff = false;
}

void YmFmCore::keyOff(int ch) {
    ChRt& c = rt_[size_t(ch)];
    for (auto& o : c.op) {
        if (o.ssgActive) {
            o.env = o.held ? o.heldAtt : (o.inv ? 512 - std::min(o.env, 512) : std::min(o.env, kMaxAtt));
            o.ssgActive = 0; o.held = 0;
        }
        if (o.stage != kOff) o.stage = kRelease;
    }
    c.keyOn = false;
}

void YmFmCore::noteOn(int ch, int note, int vel) {
    if (ch < 0 || ch >= kYmChannels) return;
    if (vel <= 0) { noteOff(ch, note); return; }
    ChRt& c = rt_[size_t(ch)];
    c.note = clampi(note, 0, 127); c.vel = clampi(vel, 1, 127);
    if (ch == 5 && gp(YmParam::DacEnable)) {
        const double pitch = gp(YmParam::DacPitched) ? std::exp2(double(c.note - gp(YmParam::DacRootNote)) / 12.0) : 1.0;
        c.dacStep = dacRate_ * pitch / sr_;
        c.dacPos = 0; c.dacPlaying = dac_ && dacLen_ > 0;
        return;
    }
    updateIncs(ch);
    keyOn(ch);
}

void YmFmCore::noteOff(int ch, int note) {
    if (ch < 0 || ch >= kYmChannels) return;
    ChRt& c = rt_[size_t(ch)];
    if (!c.keyOn || c.note != note) return;
    if (c.sustainHeld) c.pendingOff = true; else keyOff(ch);
}

void YmFmCore::pitchBend(int ch, int v14) {
    if (ch < 0 || ch >= kYmChannels) return;
    rt_[size_t(ch)].bend = float(clampi(v14, 0, 16383) - 8192) / 8192.f * kBendSemis;
    updateIncs(ch);
}

void YmFmCore::controlChange(int ch, int cc, int v) {
    if (ch < 0 || ch >= kYmChannels) return;
    ChRt& c = rt_[size_t(ch)];
    v = clampi(v, 0, 127);
    switch (cc) {
    case 1: c.ccMod = v; break;
    case 7: c.ccVol = v; break;
    case 10: c.ccPan = v; recomputePan(ch); break;
    case 11: c.ccExpr = v; break;
    case 64:
        c.sustainHeld = v >= 64;
        if (!c.sustainHeld && c.pendingOff) keyOff(ch);
        break;
    case 71: c.ccRes = v; break;
    case 74: c.ccBright = v; break;
    case 91: c.sendRev = float(v) / 127.f; break;
    case 93: c.sendCho = float(v) / 127.f; break;
    case 94: c.sendDly = float(v) / 127.f; break;
    case 120: { const bool d = c.sustainHeld; rt_[size_t(ch)] = ChRt{}; rt_[size_t(ch)].sustainHeld = d; recomputePan(ch); updateIncs(ch); } break;
    case 123: if (c.keyOn) keyOff(ch); break;
    default: break;
    }
}

void YmFmCore::allNotesOff() { for (int c = 0; c < kYmChannels; ++c) if (rt_[size_t(c)].keyOn) keyOff(c); }
void YmFmCore::allSoundsOff() {
    for (int c = 0; c < kYmChannels; ++c) {
        ChRt& r = rt_[size_t(c)];
        for (auto& o : r.op) o = OpRt{};
        r.keyOn = r.pendingOff = r.dacPlaying = false;
        r.fb1 = r.fb2 = 0;
        updateIncs(c);
    }
}

// ----------------------------------------------------------------- envelope
void YmFmCore::tickOp(OpRt& o, const YmOperatorParams& p, int kc) {
    if (o.held) return;
    const int ks = kc >> (3 - p.rs);
    switch (o.stage) {
    case kAttack: {
        const int rate = p.ar ? std::min(63, p.ar * 2 + ks) : 0;
        if (rate >= 62) o.env = 0;
        else if (rate > 0) {  // exponential approach to 0
            o.attFrac += uint32_t(o.env + 1) * (uint32_t(4 + (rate & 3)) << (rate >> 2));
            o.env -= int(o.attFrac >> 18);
            o.attFrac &= 0x3FFFF;
        }
        if (o.env <= 0) { o.env = 0; o.stage = kDecay; o.ssgActive = (p.ssg & 8) ? 1 : 0; }
        break;
    }
    case kDecay: {
        o.env += egUnits(o.frac, p.d1r ? std::min(63, p.d1r * 2 + ks) : 0);
        const int target = o.ssgActive ? 512 : slLevel(p.d1l);
        if (o.env < target) break;
        if (!o.ssgActive) { o.env = target; o.stage = kSustain; break; }
        if (p.ssg & 1) {  // hold: alternate flips once, then freeze at the end value
            if (p.ssg & 2) o.inv ^= 1;
            o.held = 1; o.heldAtt = o.inv ? 0 : kMaxAtt;
        } else {          // loop: alternate flips direction, else restart phase
            if (p.ssg & 2) o.inv ^= 1; else o.phase = 0;
            o.env = 0;
        }
        break;
    }
    case kSustain:
        o.env = std::min(kMaxAtt, o.env + egUnits(o.frac, p.d2r ? std::min(63, p.d2r * 2 + ks) : 0));
        break;
    case kRelease:
        o.env += egUnits(o.frac, std::min(63, p.rr * 4 + 2 + ks));
        if (o.env >= kMaxAtt) { o.env = kMaxAtt; o.stage = kOff; }
        break;
    default: break;
    }
}

void YmFmCore::tickEg() {
    for (int c = 0; c < kYmChannels; ++c) {
        const int kc = ymKeyCode(rt_[size_t(c)].note);
        for (int i = 0; i < kYmOps; ++i) tickOp(rt_[size_t(c)].op[size_t(i)], ch_[size_t(c)].op[size_t(i)], kc);
    }
}

int YmFmCore::operatorAttenuation(int ch, int op) const {
    const OpRt& o = rt_[size_t(ch)].op[size_t(op)];
    if (!o.ssgActive) return o.env;
    if (o.held) return o.heldAtt;
    return o.inv ? 512 - std::min(o.env, 512) : std::min(o.env, kMaxAtt);
}

// ------------------------------------------------------------------- render
void YmFmCore::renderChannel(int ch, float& outL, float& outR) {
    ChRt& c = rt_[size_t(ch)];
    const YmChannelParams& p = ch_[size_t(ch)];
    const float vol = float(c.ccVol) / 127.f, expr = float(c.ccExpr) / 127.f;
    const float chanGain = vol * vol * expr * expr;
    if (ch == 5 && gp(YmParam::DacEnable)) {
        if (!c.dacPlaying) return;
        const size_t idx = size_t(c.dacPos);
        if (idx >= dacLen_) { c.dacPlaying = false; return; }
        const float x = (float(dac_[idx]) - 128.f) / 128.f * 0.5f * chanGain * (float(c.vel) / 127.f);
        c.dacPos += c.dacStep;
        outL += x * c.gl; outR += x * c.gr;
        return;
    }
    const uint8_t cm = kCarrier[p.algorithm];
    bool alive = c.keyOn;
    for (int i = 0; i < kYmOps && !alive; ++i) alive = ((cm >> i) & 1) && c.op[size_t(i)].stage != kOff;
    if (!alive) return;

    float lfoScale = 1.f;
    int amUnits = 0;
    if (gp(YmParam::LfoEnable)) {
        const float tri = lfoPhase_ < 0.5 ? float(4 * lfoPhase_ - 1) : float(3 - 4 * lfoPhase_);
        const float cents = tri * (kPmCents[p.fms] + float(c.ccMod) / 127.f * kPmCents[4]);
        if (cents != 0.f) lfoScale = std::exp2(cents / 1200.f);
        amUnits = int((tri + 1.f) * 0.5f * kAmDb[p.ams] / 0.09375f);
    }
    const int velAtt = (127 - c.vel) * 4 * gp(YmParam::VelocitySens) / 100;
    const int brightAtt = (64 - c.ccBright) * 4;
    float amp[4], ph[4];
    for (int i = 0; i < kYmOps; ++i) {
        OpRt& o = c.op[size_t(i)];
        const YmOperatorParams& po = p.op[size_t(i)];
        const int att = clampi(operatorAttenuation(ch, i) + po.tl * 8 + (((cm >> i) & 1) ? velAtt : brightAtt) +
                               (po.am ? amUnits : 0), 0, kMaxAtt);
        amp[i] = gainTab_[size_t(att)];
        ph[i] = float(o.phase);
        o.phase += o.inc * double(lfoScale);
        o.phase -= std::floor(o.phase);
    }
    const int fb = clampi(p.feedback + (c.ccRes - 64) / 16, 0, 7);
    const float fbMod = fb > 0 ? (c.fb1 + c.fb2) * std::exp2(float(fb - 7)) : 0.f;
    float o1 = 0;
    const float out = ymEvalAlgorithm(p.algorithm, amp, ph, fbMod, kModCycles, &o1) * 0.5f * chanGain;
    c.fb2 = c.fb1; c.fb1 = o1;
    outL += out * c.gl; outR += out * c.gr;
}

void YmFmCore::render(float* left, float* right, size_t frames) {
    const float master = float(gp(YmParam::MasterGain)) / 100.f;
    const double lfoInc = double(kLfoHz[gp(YmParam::LfoRate)]) / sr_;
    for (size_t n = 0; n < frames; ++n) {
        egAcc_ += kEgRateHz / sr_;
        while (egAcc_ >= 1.0) { egAcc_ -= 1.0; tickEg(); }
        float l = 0, r = 0, rvL = 0, rvR = 0, chL = 0, chR = 0, dlL = 0, dlR = 0;
        for (int c = 0; c < kYmChannels; ++c) {
            float cl = 0, cr = 0;
            renderChannel(c, cl, cr);
            const ChRt& rt = rt_[size_t(c)];
            l += cl; r += cr;
            rvL += cl * rt.sendRev; rvR += cr * rt.sendRev;
            chL += cl * rt.sendCho; chR += cr * rt.sendCho;
            dlL += cl * rt.sendDly; dlR += cr * rt.sendDly;
        }
        lfoPhase_ += lfoInc;
        lfoPhase_ -= std::floor(lfoPhase_);
        float oL, oR;
        fx_.processSample(l * master, r * master, chL * master, chR * master, dlL * master, dlR * master,
                          rvL * master, rvR * master, oL, oR);
        left[n] = std::clamp(oL, -1.f, 1.f);
        right[n] = std::clamp(oR, -1.f, 1.f);
    }
}

}  // namespace aur::audio
