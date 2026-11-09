#include "FxReverb.hpp"

#include <algorithm>
#include <cmath>

namespace aur::audio {
namespace {
// Original delay-length set (ms): mutually non-harmonic, 29..62 ms, right channel offset for decorrelation.
constexpr double kCombMs[FxReverb::kCombs] = {29.7, 37.1, 41.1, 43.7, 47.3, 53.1, 59.3, 61.7};
constexpr double kAllpassMs[FxReverb::kAllpass] = {12.6, 10.0, 7.7, 5.1};
constexpr double kStereoSpreadMs = 0.52;
constexpr double kMaxPreDelayMs = 100.0;
constexpr float kInputGain = 0.12f;
float ftz(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }
}  // namespace

void FxReverb::init(double sr) {
    sr_ = sr;
    size_t total = 0;
    auto place = [&](int line, double ms) {
        len_[size_t(line)] = std::max<size_t>(2, size_t(ms * 0.001 * sr));
        off_[size_t(line)] = total;
        total += len_[size_t(line)];
    };
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < kCombs; ++i) place(ch * (kCombs + kAllpass) + i, kCombMs[i] + ch * kStereoSpreadMs);
        for (int i = 0; i < kAllpass; ++i) place(ch * (kCombs + kAllpass) + kCombs + i, kAllpassMs[i] + ch * kStereoSpreadMs);
    }
    place(kLines - 1, kMaxPreDelayMs);
    pool_.assign(total, 0.f);
    reset();
    setPreDelayMs(0.f);
}

void FxReverb::reset() {
    std::fill(pool_.begin(), pool_.end(), 0.f);
    idx_.fill(0); lp_.fill(0.f); preW_ = 0;
}

void FxReverb::setRoomSize(float r) { room_ = std::clamp(r, 0.f, 1.f); }
void FxReverb::setDamping(float d) { damp_ = std::clamp(d, 0.f, 1.f); }
void FxReverb::setWidth(float w) { width_ = std::clamp(w, 0.f, 1.f); }
void FxReverb::setPreDelayMs(float ms) {
    preLen_ = std::clamp<size_t>(size_t(std::max(0.f, ms) * 0.001 * sr_), 1, len_[kLines - 1] - 1);
}

void FxReverb::process(float inL, float inR, float& wetL, float& wetR) {
    // pre-delay (mono sum)
    float* pre = &pool_[off_[kLines - 1]];
    const size_t pl = len_[kLines - 1];
    const float x = (inL + inR) * 0.5f * kInputGain;
    const float in = pre[(preW_ + pl - preLen_) % pl];
    pre[preW_] = x;
    preW_ = (preW_ + 1) % pl;

    const float g = 0.70f + 0.28f * room_;
    const float d = damp_ * 0.85f;
    float out[2];
    for (int ch = 0; ch < 2; ++ch) {
        float sum = 0;
        for (int i = 0; i < kCombs; ++i) {
            const size_t line = size_t(ch * (kCombs + kAllpass) + i);
            float* b = &pool_[off_[line]];
            const float y = b[idx_[line]];
            float& lp = lp_[size_t(ch * kCombs + i)];
            lp = y * (1.f - d) + lp * d;
            b[idx_[line]] = ftz(in + lp * g);
            idx_[line] = (idx_[line] + 1) % len_[line];
            sum += y;
        }
        for (int i = 0; i < kAllpass; ++i) {
            const size_t line = size_t(ch * (kCombs + kAllpass) + kCombs + i);
            float* b = &pool_[off_[line]];
            const float y = b[idx_[line]];
            b[idx_[line]] = ftz(sum + y * 0.5f);
            idx_[line] = (idx_[line] + 1) % len_[line];
            sum = y - sum;
        }
        out[ch] = sum;
    }
    const float a = 0.5f * (1.f + width_), b = 0.5f * (1.f - width_);
    wetL = out[0] * a + out[1] * b;
    wetR = out[1] * a + out[0] * b;
}

}  // namespace aur::audio
