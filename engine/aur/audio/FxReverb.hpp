// FxReverb.hpp - stereo Schroeder-style reverb: pre-delay -> 8 damped combs -> 4 allpasses per channel.
// All delay memory lives in one pool allocated at init(). Wet-only output.
#pragma once
#include <array>
#include <cstddef>
#include <vector>

namespace aur::audio {

class FxReverb {
public:
    static constexpr int kCombs = 8, kAllpass = 4;
    void init(double sampleRate);  // allocates; not realtime-safe
    void reset();
    void setRoomSize(float r);     // 0..1, comb feedback 0.70..0.98
    void setDamping(float d);      // 0..1, high-frequency absorption in the loop
    void setPreDelayMs(float ms);  // 0..100
    void setWidth(float w);        // 0 = mono wet .. 1 = full stereo
    void process(float inL, float inR, float& wetL, float& wetR);

    template <class V> void visit(V& v) {
        v(room_); v(damp_); v(width_); v(preLen_); v(preW_); v(lp_); v(idx_);
        v.bytes(pool_.data(), pool_.size() * sizeof(float));
    }
private:
    static constexpr int kLines = 2 * (kCombs + kAllpass) + 1;  // + pre-delay line
    double sr_ = 44100;
    float room_ = 0.5f, damp_ = 0.4f, width_ = 1.f;
    size_t preLen_ = 1, preW_ = 0;
    std::array<size_t, kLines> off_{}, len_{};
    std::array<size_t, kLines> idx_{};
    std::array<float, 2 * kCombs> lp_{};
    std::vector<float> pool_;
};

}  // namespace aur::audio
