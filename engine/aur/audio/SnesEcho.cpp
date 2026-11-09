#include "SnesEcho.hpp"

#include <algorithm>
#include <cmath>

namespace aur::audio {
namespace {
float ftz(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }
}

void SnesEcho::init(double sampleRate, double maxDelayMs) {
    sr_ = sampleRate;
    buf_.assign((size_t(maxDelayMs * 0.001 * sampleRate) + 4) * 2, 0.f);
    reset();
    setDelayMs(80.f);
}

void SnesEcho::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.f);
    for (auto& h : hist_) h = {0.f, 0.f};
    w_ = 0;
}

void SnesEcho::setDelayMs(float ms) {
    const size_t cap = buf_.size() / 2 - 2;
    delay_ = std::clamp<size_t>(size_t(std::max(0.f, ms) * 0.001 * sr_), 1, cap);
}
void SnesEcho::setFeedback(float fb) { feedback_ = std::clamp(fb, -1.f, 1.f); }
void SnesEcho::setWetLevel(float l) { wet_ = std::clamp(l, 0.f, 1.f); }
void SnesEcho::setFir(const float t[kTaps]) { for (int i = 0; i < kTaps; ++i) fir_[size_t(i)] = t[i]; }
void SnesEcho::setFirHardware(const int8_t t[kTaps]) { for (int i = 0; i < kTaps; ++i) fir_[size_t(i)] = float(t[i]) / 128.f; }
void SnesEcho::setBufferWrite(bool e) { write_ = e; }

void SnesEcho::process(float inL, float inR, float& wetL, float& wetR) {
    const size_t frames = buf_.size() / 2;
    const size_t rd = (w_ + frames - delay_) % frames;
    for (int k = 0; k < kTaps - 1; ++k) hist_[size_t(k)] = hist_[size_t(k + 1)];
    hist_[kTaps - 1] = {buf_[rd * 2], buf_[rd * 2 + 1]};
    float fl = 0, fr = 0;
    for (int k = 0; k < kTaps; ++k) { fl += hist_[size_t(k)][0] * fir_[size_t(k)]; fr += hist_[size_t(k)][1] * fir_[size_t(k)]; }
    if (write_) {
        buf_[w_ * 2] = ftz(inL + fl * feedback_);
        buf_[w_ * 2 + 1] = ftz(inR + fr * feedback_);
    }
    w_ = (w_ + 1) % frames;
    wetL = fl * wet_;
    wetR = fr * wet_;
}

}  // namespace aur::audio
