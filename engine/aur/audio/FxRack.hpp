// FxRack.hpp - chorus + delay + reverb send/return rack shared by both cores.
// Routing: chorus -> (optional) reverb; delay -> (optional) reverb; out = dry + sum(level * wet).
// Disabled (default) = exact passthrough. Per-source sends come from the caller (cores feed CC91/93/94).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "FxChorusDelay.hpp"
#include "FxReverb.hpp"

namespace aur::audio {

enum class FxParam : uint8_t {
    Enable,
    ChorusRate, ChorusDepth, ChorusDelay, ChorusFeedback, ChorusVoices, ChorusSpread, ChorusLevel,
    DelayTimeL, DelayTimeR, DelayFeedback, DelayDamping, DelayPingPong, DelayLevel,
    ReverbRoom, ReverbDamping, ReverbPreDelay, ReverbWidth, ReverbLevel,
    ChorusToReverb, DelayToReverb,
    MasterSendChorus, MasterSendDelay, MasterSendReverb,
    Count
};
struct FxParamInfo { const char* name; int min; int max; int def; };

inline constexpr std::array<FxParamInfo, size_t(FxParam::Count)> kFxParamTable{{
    {"FX Enable", 0, 1, 0},
    {"Chorus Rate (0.01 Hz)", 1, 1000, 80},   {"Chorus Depth (0.1 ms)", 0, 200, 30},
    {"Chorus Delay (0.1 ms)", 3, 400, 140},   {"Chorus Feedback (%)", -95, 95, 0},
    {"Chorus Voices", 1, 4, 2},               {"Chorus Spread (%)", 0, 100, 100},
    {"Chorus Level (%)", 0, 100, 70},
    {"Delay Time L (ms)", 1, 1000, 375},      {"Delay Time R (ms)", 1, 1000, 375},
    {"Delay Feedback (%)", -95, 95, 40},      {"Delay Damping (%)", 0, 100, 30},
    {"Delay Ping-Pong", 0, 1, 0},             {"Delay Level (%)", 0, 100, 50},
    {"Reverb Room (%)", 0, 100, 50},          {"Reverb Damping (%)", 0, 100, 40},
    {"Reverb Pre-Delay (ms)", 0, 100, 0},     {"Reverb Width (%)", 0, 100, 100},
    {"Reverb Level (%)", 0, 100, 40},
    {"Chorus to Reverb (%)", 0, 100, 0},      {"Delay to Reverb (%)", 0, 100, 0},
    {"Master Send Chorus (%)", 0, 100, 0},    {"Master Send Delay (%)", 0, 100, 0},
    {"Master Send Reverb (%)", 0, 100, 0}}};

struct FxSends { float chorus = 0, delay = 0, reverb = 0; };  // linear gains, applied to the source signal

class FxRack {
public:
    void init(double sampleRate);  // allocates; not realtime-safe
    void reset();                  // clears effect memory, keeps parameters
    bool setParam(FxParam p, int value);
    int param(FxParam p) const { return v_[size_t(p)]; }
    bool enabled() const { return v_[0] != 0; }

    // dry in; `send` = per-source send gains (already summed by caller as weighted signals):
    // the caller passes the *send-bus signals* so per-channel CC91/93/94 levels can differ.
    void processSample(float dryL, float dryR, float chL, float chR, float dlL, float dlR, float rvL, float rvR,
                       float& outL, float& outR);
    // Convenience: whole-bus processing using the master send params only (in place).
    void processBlock(float* l, float* r, size_t frames);

    template <class V> void visit(V& v) {
        v(v_); chorus_.visit(v); delay_.visit(v); reverb_.visit(v);
    }
private:
    void apply(FxParam p);
    std::array<int, size_t(FxParam::Count)> v_{};
    FxChorus chorus_;
    FxDelay delay_;
    FxReverb reverb_;
};

}  // namespace aur::audio
