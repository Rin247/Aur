#include "FxChorusDelay.hpp"

#include <algorithm>
#include <cmath>

namespace aur::audio {
namespace {
constexpr double kTwoPi = 6.283185307179586;
float ftz(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }
// Linear-interpolated read of an interleaved stereo ring, `d` samples behind write index `w`.
float ringRead(const std::vector<float>& buf, size_t frames, size_t w, int ch, float d) {
    const float pos = float(w) - d;
    const float fl = std::floor(pos);
    const float fr = pos - fl;
    long i0 = long(fl) % long(frames);
    if (i0 < 0) i0 += long(frames);
    const size_t i1 = (size_t(i0) + 1) % frames;
    return buf[size_t(i0) * 2 + size_t(ch)] * (1.f - fr) + buf[i1 * 2 + size_t(ch)] * fr;
}
}  // namespace

// ------------------------------------------------------------------- chorus
void FxChorus::init(double sr) { sr_ = sr; frames_ = size_t(0.080 * sr) + 8; buf_.assign(frames_ * 2, 0.f); reset(); }
void FxChorus::reset() { std::fill(buf_.begin(), buf_.end(), 0.f); phase_ = 0; w_ = 0; }
void FxChorus::setRateHz(float v) { rate_ = std::clamp(v, 0.01f, 10.f); }
void FxChorus::setDepthMs(float v) { depth_ = std::clamp(v, 0.f, 20.f); }
void FxChorus::setDelayMs(float v) { delay_ = std::clamp(v, 0.3f, 40.f); }
void FxChorus::setFeedback(float v) { fb_ = std::clamp(v, -0.95f, 0.95f); }
void FxChorus::setVoices(int v) { voices_ = std::clamp(v, 1, kMaxVoices); }
void FxChorus::setSpread(float v) { spread_ = std::clamp(v, 0.f, 1.f); }
float FxChorus::read(int ch, float d) const { return ringRead(buf_, frames_, w_, ch, d); }

void FxChorus::process(float inL, float inR, float& wetL, float& wetR) {
    const float baseS = float(delay_ * 0.001 * sr_), depthS = float(depth_ * 0.001 * sr_);
    const float maxD = float(frames_) - 3.f;
    float aL = 0, aR = 0;
    for (int v = 0; v < voices_; ++v) {
        const double ph = phase_ + double(v) / double(voices_);
        const float sl = float(std::sin(kTwoPi * ph));
        const float sr = float(std::sin(kTwoPi * (ph + double(spread_) * 0.25)));
        aL += read(0, std::clamp(baseS + depthS * sl, 1.f, maxD));
        aR += read(1, std::clamp(baseS + depthS * sr, 1.f, maxD));
    }
    const float g = 1.f / std::sqrt(float(voices_));
    wetL = aL * g; wetR = aR * g;
    buf_[w_ * 2] = ftz(inL + fb_ * wetL);
    buf_[w_ * 2 + 1] = ftz(inR + fb_ * wetR);
    w_ = (w_ + 1) % frames_;
    phase_ += double(rate_) / sr_;
    phase_ -= std::floor(phase_);
}

// -------------------------------------------------------------------- delay
void FxDelay::init(double sr, double maxMs) {
    sr_ = sr; frames_ = size_t(maxMs * 0.001 * sr) + 8; buf_.assign(frames_ * 2, 0.f);
    setTimeMs(375.f, 375.f); curL_ = tgtL_; curR_ = tgtR_;
    reset();
}
void FxDelay::reset() { std::fill(buf_.begin(), buf_.end(), 0.f); w_ = 0; lpL_ = lpR_ = 0; }
void FxDelay::setTimeMs(float l, float r) {
    const float mx = float(frames_) - 4.f;
    tgtL_ = std::clamp(float(l * 0.001 * sr_), 1.f, mx);
    tgtR_ = std::clamp(float(r * 0.001 * sr_), 1.f, mx);
}
void FxDelay::setFeedback(float v) { fb_ = std::clamp(v, -0.95f, 0.95f); }
void FxDelay::setDamping(float v) { damp_ = std::clamp(v, 0.f, 1.f); }
void FxDelay::setPingPong(bool on) { pp_ = on; }
float FxDelay::read(int ch, float d) const { return ringRead(buf_, frames_, w_, ch, d); }

void FxDelay::process(float inL, float inR, float& wetL, float& wetR) {
    // slew time changes so edits glide instead of clicking; snap when close so steady state is exact
    for (float* c : {&curL_, &curR_}) {
        const float t = (c == &curL_) ? tgtL_ : tgtR_;
        *c += (t - *c) * 0.002f;
        if (std::fabs(t - *c) < 0.5f) *c = t;  // float steps stall near the target, so snap within half a sample
    }
    wetL = read(0, curL_);
    wetR = read(1, curR_);
    const float a = 1.f - damp_ * 0.9f;
    lpL_ += a * (wetL - lpL_);
    lpR_ += a * (wetR - lpR_);
    if (pp_) {
        buf_[w_ * 2] = ftz(0.5f * (inL + inR) + fb_ * lpR_);
        buf_[w_ * 2 + 1] = ftz(fb_ * lpL_);
    } else {
        buf_[w_ * 2] = ftz(inL + fb_ * lpL_);
        buf_[w_ * 2 + 1] = ftz(inR + fb_ * lpR_);
    }
    w_ = (w_ + 1) % frames_;
}

}  // namespace aur::audio
