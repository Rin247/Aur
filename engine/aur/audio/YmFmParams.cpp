// YmFmParams.cpp - YmFmCore parameter setters/getters, patch bytes (raw + sysex frame), state save/load.
#include <algorithm>
#include <cmath>

#include "YmFmCore.hpp"
#include "YmFmPatches.hpp"

namespace aur::audio {
namespace {
constexpr uint32_t kStateMagic = 0x594D3236;  // "YM26"
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

// ---------------------------------------------------------------- parameters
bool YmFmCore::setGlobalParam(YmParam p, int v) {
    const auto& info = kYmParamTable[size_t(p)];
    if (info.scope != YmParamScope::Global) return false;
    g_[size_t(p)] = clampi(v, info.min, info.max);
    if (p == YmParam::DacEnable) { rt_[5] = ChRt{}; recomputePan(5); }
    if (p == YmParam::SoftPan) for (int c = 0; c < kYmChannels; ++c) recomputePan(c);
    return true;
}
int YmFmCore::globalParam(YmParam p) const { return g_[size_t(p)]; }

bool YmFmCore::setChannelParam(int ch, YmParam p, int v) {
    if (ch < 0 || ch >= kYmChannels) return false;
    const auto& info = kYmParamTable[size_t(p)];
    if (info.scope != YmParamScope::Channel) return false;
    v = clampi(v, info.min, info.max);
    YmChannelParams& c = ch_[size_t(ch)];
    switch (p) {
    case YmParam::Algorithm: c.algorithm = v; break;
    case YmParam::Feedback: c.feedback = v; break;
    case YmParam::Pan: c.pan = v; recomputePan(ch); break;
    case YmParam::Ams: c.ams = v; break;
    default: c.fms = v; break;
    }
    return true;
}
int YmFmCore::channelParam(int ch, YmParam p) const {
    const YmChannelParams& c = ch_[size_t(ch)];
    switch (p) {
    case YmParam::Algorithm: return c.algorithm;
    case YmParam::Feedback: return c.feedback;
    case YmParam::Pan: return c.pan;
    case YmParam::Ams: return c.ams;
    default: return c.fms;
    }
}

bool YmFmCore::setOperatorParam(int ch, int op, YmParam p, int v) {
    if (ch < 0 || ch >= kYmChannels || op < 0 || op >= kYmOps) return false;
    const auto& info = kYmParamTable[size_t(p)];
    if (info.scope != YmParamScope::Operator) return false;
    v = clampi(v, info.min, info.max);
    YmOperatorParams& o = ch_[size_t(ch)].op[size_t(op)];
    switch (p) {
    case YmParam::OpDt: o.dt = v; break;
    case YmParam::OpMul: o.mul = v; break;
    case YmParam::OpTl: o.tl = v; break;
    case YmParam::OpRs: o.rs = v; break;
    case YmParam::OpAr: o.ar = v; break;
    case YmParam::OpAm: o.am = v; break;
    case YmParam::OpD1r: o.d1r = v; break;
    case YmParam::OpD2r: o.d2r = v; break;
    case YmParam::OpD1l: o.d1l = v; break;
    case YmParam::OpRr: o.rr = v; break;
    default: o.ssg = v; break;
    }
    updateIncs(ch);
    return true;
}
int YmFmCore::operatorParam(int ch, int op, YmParam p) const {
    const YmOperatorParams& o = ch_[size_t(ch)].op[size_t(op)];
    switch (p) {
    case YmParam::OpDt: return o.dt;
    case YmParam::OpMul: return o.mul;
    case YmParam::OpTl: return o.tl;
    case YmParam::OpRs: return o.rs;
    case YmParam::OpAr: return o.ar;
    case YmParam::OpAm: return o.am;
    case YmParam::OpD1r: return o.d1r;
    case YmParam::OpD2r: return o.d2r;
    case YmParam::OpD1l: return o.d1l;
    case YmParam::OpRr: return o.rr;
    default: return o.ssg;
    }
}

void YmFmCore::setChannelParams(int ch, const YmChannelParams& p) {
    if (ch < 0 || ch >= kYmChannels) return;
    YmChannelParams& c = ch_[size_t(ch)];
    c = p;
    c.algorithm = clampi(c.algorithm, 0, 7); c.feedback = clampi(c.feedback, 0, 7);
    c.pan = clampi(c.pan, 0, 3); c.ams = clampi(c.ams, 0, 3); c.fms = clampi(c.fms, 0, 7);
    for (auto& o : c.op) {
        o.dt = clampi(o.dt, 0, 7); o.mul = clampi(o.mul, 0, 15); o.tl = clampi(o.tl, 0, 127);
        o.rs = clampi(o.rs, 0, 3); o.ar = clampi(o.ar, 0, 31); o.am = clampi(o.am, 0, 1);
        o.d1r = clampi(o.d1r, 0, 31); o.d2r = clampi(o.d2r, 0, 31); o.d1l = clampi(o.d1l, 0, 15);
        o.rr = clampi(o.rr, 0, 15); o.ssg = clampi(o.ssg, 0, 15);
    }
    recomputePan(ch);
    updateIncs(ch);
}

bool YmFmCore::loadBuiltinPatch(int ch, int index) {
    if (ch < 0 || ch >= kYmChannels || index < 0 || index >= kYmBuiltinPatchCount) return false;
    setChannelParams(ch, ymBuiltinPatch(index).params);
    return true;
}

// Patch bytes: alg,fb,ams,fms then per op dt,mul,tl,rs,ar,am,d1r,d2r,d1l,rr,ssg. Sysex frame is 7-bit safe.
bool YmFmCore::loadPatchBytes(int ch, std::span<const uint8_t> b) {
    if (ch < 0 || ch >= kYmChannels) return false;
    if (b.size() == size_t(kYmSysexBytes)) {
        if (b[0] != 0xF0 || b[1] != 0x7D || b[2] != 0x59 || b[3] != 0x4D || b[4] != 0x01 || b[54] != 0xF7) return false;
        int sum = 0;
        for (int i = 5; i < 53; ++i) sum += b[size_t(i)];
        if (uint8_t((-sum) & 0x7F) != b[53]) return false;
        b = b.subspan(5, kYmPatchBytes);
    }
    if (b.size() != size_t(kYmPatchBytes)) return false;
    YmChannelParams p = ch_[size_t(ch)];
    p.algorithm = b[0]; p.feedback = b[1]; p.ams = b[2]; p.fms = b[3];
    for (int i = 0; i < kYmOps; ++i) {
        const uint8_t* s = &b[size_t(4 + i * 11)];
        YmOperatorParams& o = p.op[size_t(i)];
        o = YmOperatorParams{s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[10]};
    }
    // Reject out-of-range input instead of silently clamping.
    const int ranges[4] = {7, 7, 3, 7};
    for (int i = 0; i < 4; ++i) if (b[size_t(i)] > ranges[i]) return false;
    const int mx[11] = {7, 15, 127, 3, 31, 1, 31, 31, 15, 15, 15};
    for (int i = 0; i < kYmOps; ++i)
        for (int k = 0; k < 11; ++k) if (b[size_t(4 + i * 11 + k)] > mx[k]) return false;
    setChannelParams(ch, p);
    return true;
}

size_t YmFmCore::patchToBytes(const YmChannelParams& p, std::span<uint8_t> out, bool frame) {
    const size_t need = frame ? size_t(kYmSysexBytes) : size_t(kYmPatchBytes);
    if (out.size() < need) return 0;
    uint8_t* d = out.data() + (frame ? 5 : 0);
    if (frame) { out[0] = 0xF0; out[1] = 0x7D; out[2] = 0x59; out[3] = 0x4D; out[4] = 0x01; }
    d[0] = uint8_t(p.algorithm); d[1] = uint8_t(p.feedback); d[2] = uint8_t(p.ams); d[3] = uint8_t(p.fms);
    for (int i = 0; i < kYmOps; ++i) {
        const YmOperatorParams& o = p.op[size_t(i)];
        const int v[11] = {o.dt, o.mul, o.tl, o.rs, o.ar, o.am, o.d1r, o.d2r, o.d1l, o.rr, o.ssg};
        for (int k = 0; k < 11; ++k) d[4 + i * 11 + k] = uint8_t(v[k]);
    }
    if (frame) {
        int sum = 0;
        for (int i = 0; i < kYmPatchBytes; ++i) sum += d[i];
        out[53] = uint8_t((-sum) & 0x7F); out[54] = 0xF7;
    }
    return need;
}

// -------------------------------------------------------------------- state
template <class V> void YmFmCore::visit(V& v) {
    v(g_); v(ch_); v(rt_); v(lfoPhase_); v(egAcc_);
    fx_.visit(v);
}
size_t YmFmCore::stateSize() { StateSizer s; visit(s); return s.size() + sizeof(kStateMagic); }
bool YmFmCore::saveState(uint8_t* dst, size_t cap, size_t* written) {
    StateWriter w(dst, cap);
    uint32_t m = kStateMagic;
    w(m); visit(w);
    if (written) *written = w.size();
    return w.ok();
}
bool YmFmCore::loadState(const uint8_t* src, size_t len) {
    StateReader r(src, len);
    uint32_t m = 0;
    r(m);
    if (!r.ok() || m != kStateMagic) return false;
    if (len != stateSize()) return false;  // exact size required: nothing is modified on mismatch
    visit(r);
    return r.ok();
}

}  // namespace aur::audio
