// FxChorusDelay.hpp - modulated-delay chorus/flanger and a stereo (ping-pong) delay. Wet-only outputs.
#pragma once
#include <cstddef>
#include <vector>

namespace aur::audio {

class FxChorus {
public:
    static constexpr int kMaxVoices = 4;
    void init(double sampleRate);  // allocates 80 ms stereo line; not realtime-safe
    void reset();
    void setRateHz(float hz);       // 0.01..10
    void setDepthMs(float ms);      // modulation excursion, 0..20
    void setDelayMs(float ms);      // base delay: ~10-25 chorus, ~0.5-3 flanger
    void setFeedback(float fb);     // -0.95..0.95
    void setVoices(int n);          // 1..4, phases evenly spread
    void setSpread(float s);        // 0..1 L/R LFO quadrature amount
    void process(float inL, float inR, float& wetL, float& wetR);

    template <class V> void visit(V& v) {
        v(phase_); v(w_); v(rate_); v(depth_); v(delay_); v(fb_); v(voices_); v(spread_);
        v.bytes(buf_.data(), buf_.size() * sizeof(float));
    }
private:
    float read(int ch, float delaySamples) const;
    double sr_ = 44100, phase_ = 0;
    size_t frames_ = 0, w_ = 0;
    float rate_ = 0.8f, depth_ = 3.f, delay_ = 14.f, fb_ = 0.f, spread_ = 1.f;
    int voices_ = 2;
    std::vector<float> buf_;
};

class FxDelay {
public:
    void init(double sampleRate, double maxMs = 1500.0);  // allocates; not realtime-safe
    void reset();
    void setTimeMs(float leftMs, float rightMs);  // glides to the new time (no clicks)
    void jumpToTarget() { curL_ = tgtL_; curR_ = tgtR_; }  // skip the glide (preset load)
    void setFeedback(float fb);   // -0.95..0.95
    void setDamping(float d);     // 0..1 one-pole lowpass in the feedback path
    void setPingPong(bool on);    // mono input bounces L<->R
    void process(float inL, float inR, float& wetL, float& wetR);

    template <class V> void visit(V& v) {
        v(w_); v(curL_); v(curR_); v(tgtL_); v(tgtR_); v(fb_); v(damp_); v(pp_); v(lpL_); v(lpR_);
        v.bytes(buf_.data(), buf_.size() * sizeof(float));
    }
private:
    float read(int ch, float d) const;
    double sr_ = 44100;
    size_t frames_ = 0, w_ = 0;
    float curL_ = 0, curR_ = 0, tgtL_ = 0, tgtR_ = 0, fb_ = 0.4f, damp_ = 0.3f, lpL_ = 0, lpR_ = 0;
    bool pp_ = false;
    std::vector<float> buf_;
};

}  // namespace aur::audio
