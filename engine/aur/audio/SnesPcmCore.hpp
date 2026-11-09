// SnesPcmCore.hpp - SPC700/S-DSP-inspired sampler core.
// Default 8 voices (chip limit). To raise it: setGlobalParam(SnesParam::VoiceLimit, n) up to kSnesMaxVoices (32)
// or raise kSnesMaxVoices at compile time; every extra voice is a non-authentic extension.
// MIDI channels (0..15) select a channel setup (sample slot, ADSR/GAIN, pan...). Echo is a separate post-voice module.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "CoreState.hpp"
#include "FxRack.hpp"
#include "SnesEcho.hpp"

namespace aur::audio {

inline constexpr int kSnesDefaultVoices = 8;
inline constexpr int kSnesMaxVoices = 32;
inline constexpr int kSnesChannels = 16;
inline constexpr int kSnesMaxSlots = 32;
inline constexpr int kSnesDemoSamples = 3;  // slots 0..2 after loadDemoSamples(): saw, pulse, snare

struct SnesChannelSetup {
    int slot = 0;
    int attack = 15, decay = 7, sustainLevel = 7, sustainRate = 0;  // ADSR (4/3/3/5 bits)
    int useGain = 0, gain = 127;                                    // GAIN byte semantics of the chip
    int pan = 64, volume = 100, echoSend = 0, tuneCents = 0;
};

enum class SnesParam : uint8_t {
    VoiceLimit, MasterVolume, EchoDelayMs, EchoFeedback, EchoWet, EchoWriteEnable,
    FirTap0, FirTap1, FirTap2, FirTap3, FirTap4, FirTap5, FirTap6, FirTap7,  // global
    Slot, Attack, Decay, SustainLevel, SustainRate, UseGain, GainValue, Pan, Volume, EchoSend, TuneCents,  // channel
    Count
};
enum class SnesParamScope : uint8_t { Global, Channel };
struct SnesParamInfo { const char* name; int min; int max; int def; SnesParamScope scope; };

inline constexpr int kSnesGlobalParams = 14;
inline constexpr std::array<SnesParamInfo, size_t(SnesParam::Count)> kSnesParamTable{{
    {"Voice Limit", 1, kSnesMaxVoices, kSnesDefaultVoices, SnesParamScope::Global},
    {"Master Volume", 0, 100, 50, SnesParamScope::Global},
    {"Echo Delay (ms)", 0, 240, 80, SnesParamScope::Global},
    {"Echo Feedback (%)", -100, 100, 40, SnesParamScope::Global},
    {"Echo Wet (%)", 0, 100, 35, SnesParamScope::Global},
    {"Echo Write Enable", 0, 1, 1, SnesParamScope::Global},
    {"FIR Tap 0 (oldest)", -128, 127, 4, SnesParamScope::Global}, {"FIR Tap 1", -128, 127, 12, SnesParamScope::Global},
    {"FIR Tap 2", -128, 127, 24, SnesParamScope::Global},         {"FIR Tap 3", -128, 127, 32, SnesParamScope::Global},
    {"FIR Tap 4", -128, 127, 28, SnesParamScope::Global},         {"FIR Tap 5", -128, 127, 16, SnesParamScope::Global},
    {"FIR Tap 6", -128, 127, 8, SnesParamScope::Global},          {"FIR Tap 7 (newest)", -128, 127, 4, SnesParamScope::Global},
    {"Sample Slot", 0, kSnesMaxSlots - 1, 0, SnesParamScope::Channel},
    {"Attack", 0, 15, 15, SnesParamScope::Channel},          {"Decay", 0, 7, 7, SnesParamScope::Channel},
    {"Sustain Level", 0, 7, 7, SnesParamScope::Channel},     {"Sustain Rate", 0, 31, 0, SnesParamScope::Channel},
    {"Use GAIN", 0, 1, 0, SnesParamScope::Channel},          {"GAIN Value", 0, 255, 127, SnesParamScope::Channel},
    {"Pan", 0, 127, 64, SnesParamScope::Channel},            {"Volume", 0, 127, 100, SnesParamScope::Channel},
    {"Echo Send", 0, 1, 0, SnesParamScope::Channel},         {"Tune (cents)", -100, 100, 0, SnesParamScope::Channel}}};

// Gaussian interpolation kernel: 512 entries; taps for fractional index i (0..255) are
// table[255-i], table[511-i], table[256+i], table[i] (sum ~ 2048). The built table is a fitted
// Gaussian, not the silicon ROM; load the exact table with setGaussTable() if you have a licensed source.
void snesBuildGaussTable(int16_t out[512], double sigma = 0.63);
void snesGaussTaps(const int16_t table[512], int fracIndex, int outTaps[4]);

class SnesPcmCore {
public:
    void init(double sampleRate, size_t samplePoolFrames = size_t(1) << 20);  // allocates; not realtime-safe
    void reset();  // keeps loaded samples, resets parameters/state

    // Samples: host data is copied into the core-owned pool (call off the audio thread). Returns slot or -1.
    int loadPcm16(const int16_t* data, size_t frames, double srcRateHz, int rootNote, bool loop, size_t loopStart);
    int loadBrr(const uint8_t* data, size_t blocks, double srcRateHz, int rootNote, int loopBlock);  // loopBlock<0: loop only if the end block says so
    int loadDemoSamples();  // generates saw/pulse/snare procedurally; returns first slot or -1
    void clearSamples();
    int sampleSlots() const { return nSlots_; }
    void setGaussTable(const int16_t table[512]);

    bool setGlobalParam(SnesParam p, int v);
    int globalParam(SnesParam p) const { return g_[size_t(p)]; }
    bool setChannelParam(int ch, SnesParam p, int v);
    int channelParam(int ch, SnesParam p) const;
    void setChannelSetup(int ch, const SnesChannelSetup& s);
    const SnesChannelSetup& channelSetup(int ch) const { return setup_[size_t(ch)]; }

    void noteOn(int ch, int note, int velocity);
    void noteOff(int ch, int note);
    void pitchBend(int ch, int value14);  // +-2 semitones
    void controlChange(int ch, int cc, int value);  // 1,7,10,11,64,71,74,91,93,94,120,123
    void allNotesOff();
    void allSoundsOff();  // silences voices; echo tail is kept (use reset() to clear it)

    void render(float* left, float* right, size_t frames);  // dry + echo + FX rack, overwrites buffers
    // Dry voices and echo wet on separate buses (FX rack bypassed); overwrites all four buffers.
    void renderSplit(float* dryL, float* dryR, float* wetL, float* wetR, size_t frames);

    int activeVoices() const;
    int voiceEnvelope(int voice) const { return voices_[size_t(voice)].env; }  // 0..2047, for tests/UI
    FxRack& fx() { return fx_; }

    size_t stateSize();  // excludes sample pool and Gaussian table (host re-supplies them)
    bool saveState(uint8_t* dst, size_t capacity, size_t* written);
    bool loadState(const uint8_t* src, size_t length);

private:
    enum Stage : uint8_t { kOff, kAttack, kDecay, kSustain, kRelease, kGain };
    struct Slot { size_t off = 0, len = 0, loopStart = 0; double rate = 32000; int root = 60; bool loop = false; };
    struct Voice {
        bool active = false, released = false, sustainHeld = false;
        uint8_t stage = kOff;
        int ch = 0, note = 60, vel = 100, slot = 0, env = 0, ar = 15, dr = 7, sl = 7, sr = 0, useGain = 0, gain = 127, echo = 0, tune = 0, counter = 0;
        uint64_t age = 0;
        double pos = 0, step = 0;
        float vib = 0, ic1 = 0, ic2 = 0;
    };
    struct Chan {
        int expr = 127, mod = 0, bright = 127, res = 0;
        bool sustain = false;
        float bend = 0, gl = 1, gr = 1, gain = 1, rev = 0, cho = 0, dly = 0, a1 = 0, a2 = 0, a3 = 0, k = 0;
    };
    struct Bus { float rvL = 0, rvR = 0, chL = 0, chR = 0, dlL = 0, dlR = 0; };

    template <class V> void visit(V& v);
    int storeSlot(size_t frames, double rate, int root, bool loop, size_t loopStart);
    void recompute(int ch);
    void tickEnv(Voice& v);
    float interp(const Slot& s, double pos) const;
    void renderFrame(float& dL, float& dR, float& wL, float& wR, Bus& bus);
    void release(Voice& v) { v.released = true; v.stage = kRelease; }

    double sr_ = 44100;
    std::array<int, kSnesGlobalParams> g_{};
    std::array<SnesChannelSetup, kSnesChannels> setup_{};
    std::array<Chan, kSnesChannels> chan_{};
    std::array<Voice, kSnesMaxVoices> voices_{};
    std::array<Slot, kSnesMaxSlots> slots_{};
    int nSlots_ = 0;
    size_t poolUsed_ = 0;
    uint64_t ageCounter_ = 0;
    uint32_t egCounter_ = 0;
    double egAcc_ = 0;
    std::vector<int16_t> pool_;
    std::array<int16_t, 512> gauss_{};
    SnesEcho echo_;
    FxRack fx_;
};

}  // namespace aur::audio
