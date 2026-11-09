// SnesEcho.hpp - S-DSP style echo: ring buffer, 8-tap FIR over the delayed signal, feedback.
// Wet-only: process() returns just the echo signal; the caller mixes dry + wet.
// The FIR taps are consecutive *host-rate* samples (the chip runs them at 32 kHz).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aur::audio {

class SnesEcho {
public:
    static constexpr int kTaps = 8;

    void init(double sampleRate, double maxDelayMs = 240.0);  // allocates; not realtime-safe
    void reset();                                             // clears buffer + FIR history

    void setDelayMs(float ms);
    void setFeedback(float fb);                      // -1..1
    void setWetLevel(float level);                   // 0..1 (chip EVOL)
    void setFir(const float taps[kTaps]);            // taps[0] = oldest sample, taps[7] = newest
    void setFirHardware(const int8_t taps[kTaps]);   // signed 8-bit, /128
    void setBufferWrite(bool enabled);               // false = buffer frozen (chip FLG echo-write off)
    size_t delaySamples() const { return delay_; }

    // `in` is the echo send (voices with echo enabled). Adds nothing to dry.
    void process(float inL, float inR, float& wetL, float& wetR);

    template <class V> void visit(V& v) {
        v(delay_); v(feedback_); v(wet_); v(fir_); v(write_); v(w_); v(hist_);
        v.bytes(buf_.data(), buf_.size() * sizeof(float));
    }

private:
    double sr_ = 44100;
    size_t delay_ = 1, w_ = 0;
    float feedback_ = 0.4f, wet_ = 0.35f;
    std::array<float, kTaps> fir_{4 / 128.f, 12 / 128.f, 24 / 128.f, 32 / 128.f, 28 / 128.f, 16 / 128.f, 8 / 128.f, 4 / 128.f};
    bool write_ = true;
    std::array<std::array<float, 2>, kTaps> hist_{};
    std::vector<float> buf_;  // interleaved L,R; size fixed at init
};

}  // namespace aur::audio
