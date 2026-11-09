#include "FxRack.hpp"

#include <algorithm>

namespace aur::audio {

void FxRack::init(double sr) {
    chorus_.init(sr); delay_.init(sr); reverb_.init(sr);
    for (size_t i = 0; i < v_.size(); ++i) v_[i] = kFxParamTable[i].def;
    for (size_t i = 1; i < v_.size(); ++i) apply(FxParam(i));
}

void FxRack::reset() { chorus_.reset(); delay_.reset(); reverb_.reset(); }

bool FxRack::setParam(FxParam p, int value) {
    if (size_t(p) >= v_.size()) return false;
    const auto& info = kFxParamTable[size_t(p)];
    v_[size_t(p)] = std::clamp(value, info.min, info.max);
    apply(p);
    return true;
}

void FxRack::apply(FxParam p) {
    auto f = [&](FxParam q) { return float(v_[size_t(q)]); };
    switch (p) {
    case FxParam::ChorusRate: chorus_.setRateHz(f(p) * 0.01f); break;
    case FxParam::ChorusDepth: chorus_.setDepthMs(f(p) * 0.1f); break;
    case FxParam::ChorusDelay: chorus_.setDelayMs(f(p) * 0.1f); break;
    case FxParam::ChorusFeedback: chorus_.setFeedback(f(p) * 0.01f); break;
    case FxParam::ChorusVoices: chorus_.setVoices(v_[size_t(p)]); break;
    case FxParam::ChorusSpread: chorus_.setSpread(f(p) * 0.01f); break;
    case FxParam::DelayTimeL: case FxParam::DelayTimeR:
        delay_.setTimeMs(f(FxParam::DelayTimeL), f(FxParam::DelayTimeR)); break;
    case FxParam::DelayFeedback: delay_.setFeedback(f(p) * 0.01f); break;
    case FxParam::DelayDamping: delay_.setDamping(f(p) * 0.01f); break;
    case FxParam::DelayPingPong: delay_.setPingPong(v_[size_t(p)] != 0); break;
    case FxParam::ReverbRoom: reverb_.setRoomSize(f(p) * 0.01f); break;
    case FxParam::ReverbDamping: reverb_.setDamping(f(p) * 0.01f); break;
    case FxParam::ReverbPreDelay: reverb_.setPreDelayMs(f(p)); break;
    case FxParam::ReverbWidth: reverb_.setWidth(f(p) * 0.01f); break;
    default: break;  // levels / routing / sends are read directly in processSample
    }
}

void FxRack::processSample(float dryL, float dryR, float chL, float chR, float dlL, float dlR, float rvL, float rvR,
                           float& outL, float& outR) {
    if (!enabled()) { outL = dryL; outR = dryR; return; }
    auto pct = [&](FxParam q) { return float(v_[size_t(q)]) * 0.01f; };
    float cwL, cwR, dwL, dwR, rwL, rwR;
    chorus_.process(chL, chR, cwL, cwR);
    delay_.process(dlL, dlR, dwL, dwR);
    const float c2r = pct(FxParam::ChorusToReverb), d2r = pct(FxParam::DelayToReverb);
    reverb_.process(rvL + cwL * c2r + dwL * d2r, rvR + cwR * c2r + dwR * d2r, rwL, rwR);
    const float cl = pct(FxParam::ChorusLevel), dl = pct(FxParam::DelayLevel), rl = pct(FxParam::ReverbLevel);
    outL = dryL + cwL * cl + dwL * dl + rwL * rl;
    outR = dryR + cwR * cl + dwR * dl + rwR * rl;
}

void FxRack::processBlock(float* l, float* r, size_t n) {
    if (!enabled()) return;
    const float sc = float(v_[size_t(FxParam::MasterSendChorus)]) * 0.01f;
    const float sd = float(v_[size_t(FxParam::MasterSendDelay)]) * 0.01f;
    const float sr = float(v_[size_t(FxParam::MasterSendReverb)]) * 0.01f;
    for (size_t i = 0; i < n; ++i) {
        const float a = l[i], b = r[i];
        processSample(a, b, a * sc, b * sc, a * sd, b * sd, a * sr, b * sr, l[i], r[i]);
    }
}

}  // namespace aur::audio
