// YmFmCore.hpp - YM2612-style FM core: 6 channels x 4 operators, DAC on ch6.
// Not bit-exact to silicon; timing/levels follow the documented chip behaviour.
// Channel index is the FM channel (0..5), NOT a MIDI channel. Channels are
// monophonic: a new noteOn retriggers, noteOff only releases a matching note.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "CoreState.hpp"
#include "FxRack.hpp"

namespace aur::audio {

inline constexpr int kYmChannels = 6;
inline constexpr int kYmOps = 4;
inline constexpr int kYmPatchBytes = 48;  // raw layout: alg,fb,ams,fms, then 4 x {dt,mul,tl,rs,ar,am,d1r,d2r,d1l,rr,ssg}
inline constexpr int kYmSysexBytes = 55;  // F0 7D 59 4D 01 <48 bytes> checksum F7

struct YmOperatorParams {
    int dt = 0;    // 0..7 (bit2 = negative)
    int mul = 1;   // 0..15 (0 = x0.5)
    int tl = 0;    // 0..127, 0.75 dB/step
    int rs = 0;    // key scale 0..3
    int ar = 31;   // 0..31
    int am = 0;    // LFO amplitude enable
    int d1r = 0;   // 0..31 (DR)
    int d2r = 0;   // 0..31 (SR)
    int d1l = 0;   // 0..15 (SL)
    int rr = 7;    // 0..15
    int ssg = 0;   // bit3 enable; bit2 invert-start, bit1 alternate, bit0 hold
};

struct YmChannelParams {
    int algorithm = 0;  // 0..7
    int feedback = 0;   // 0..7 (op1)
    int pan = 3;        // bit1 = left, bit0 = right
    int ams = 0;        // 0..3
    int fms = 0;        // 0..7
    std::array<YmOperatorParams, kYmOps> op{};
};

enum class YmParam : uint8_t {
    LfoEnable, LfoRate, MasterGain, SoftPan, VelocitySens, DacEnable, DacRootNote, DacPitched,  // global
    Algorithm, Feedback, Pan, Ams, Fms,                                                          // channel
    OpDt, OpMul, OpTl, OpRs, OpAr, OpAm, OpD1r, OpD2r, OpD1l, OpRr, OpSsg,                       // operator
    Count
};
enum class YmParamScope : uint8_t { Global, Channel, Operator };
struct YmParamInfo { const char* name; int min; int max; int def; YmParamScope scope; };

inline constexpr std::array<YmParamInfo, size_t(YmParam::Count)> kYmParamTable{{
    {"LFO Enable", 0, 1, 0, YmParamScope::Global},     {"LFO Rate", 0, 7, 0, YmParamScope::Global},
    {"Master Gain", 0, 100, 50, YmParamScope::Global}, {"Soft Pan", 0, 1, 0, YmParamScope::Global},
    {"Velocity Sens", 0, 100, 60, YmParamScope::Global}, {"DAC Enable", 0, 1, 0, YmParamScope::Global},
    {"DAC Root Note", 0, 127, 60, YmParamScope::Global}, {"DAC Pitched", 0, 1, 0, YmParamScope::Global},
    {"Algorithm", 0, 7, 0, YmParamScope::Channel},     {"Feedback", 0, 7, 0, YmParamScope::Channel},
    {"Pan LR", 0, 3, 3, YmParamScope::Channel},        {"AMS", 0, 3, 0, YmParamScope::Channel},
    {"FMS", 0, 7, 0, YmParamScope::Channel},
    {"DT", 0, 7, 0, YmParamScope::Operator},   {"MUL", 0, 15, 1, YmParamScope::Operator},
    {"TL", 0, 127, 0, YmParamScope::Operator}, {"RS", 0, 3, 0, YmParamScope::Operator},
    {"AR", 0, 31, 31, YmParamScope::Operator}, {"AM", 0, 1, 0, YmParamScope::Operator},
    {"D1R", 0, 31, 0, YmParamScope::Operator}, {"D2R", 0, 31, 0, YmParamScope::Operator},
    {"D1L", 0, 15, 0, YmParamScope::Operator}, {"RR", 0, 15, 7, YmParamScope::Operator},
    {"SSG-EG", 0, 15, 0, YmParamScope::Operator}}};

// Pure helpers (exposed for tests).
// amp/phase are per-operator (phase in cycles). modScale = cycles of phase shift per unit of modulator output.
float ymEvalAlgorithm(int alg, const float amp[4], const float phase[4], float fbMod, float modScale, float* op1Out);
int ymKeyCode(int note);
uint8_t ymCarrierMask(int alg);  // bit i = operator i is a carrier

class YmFmCore {
public:
    void init(double sampleRate);  // call once; not realtime-safe
    void reset();

    bool setGlobalParam(YmParam p, int value);
    int globalParam(YmParam p) const;
    bool setChannelParam(int ch, YmParam p, int value);
    int channelParam(int ch, YmParam p) const;
    bool setOperatorParam(int ch, int op, YmParam p, int value);
    int operatorParam(int ch, int op, YmParam p) const;
    void setChannelParams(int ch, const YmChannelParams& params);
    const YmChannelParams& channelParams(int ch) const { return ch_[size_t(ch)]; }
    bool loadPatchBytes(int ch, std::span<const uint8_t> bytes);  // raw 48 bytes or 55-byte framed sysex
    static size_t patchToBytes(const YmChannelParams& p, std::span<uint8_t> out, bool sysexFrame);
    bool loadBuiltinPatch(int ch, int index);

    // Host-owned 8-bit unsigned PCM; must outlive playback.
    void setDacBuffer(const uint8_t* data, size_t length, double baseRateHz);

    void noteOn(int ch, int note, int velocity);
    void noteOff(int ch, int note);
    void pitchBend(int ch, int value14);  // 0..16383, centre 8192, +-2 semitones
    void controlChange(int ch, int cc, int value);
    void allNotesOff();
    void allSoundsOff();

    void render(float* left, float* right, size_t frames);  // overwrites buffers

    size_t stateSize();
    bool saveState(uint8_t* dst, size_t capacity, size_t* written);
    bool loadState(const uint8_t* src, size_t length);

    FxRack& fx() { return fx_; }  // chorus/delay/reverb; disabled (exact bypass) by default; CC91/93/94 are per-channel sends

    int operatorAttenuation(int ch, int op) const;  // 0..1023, test/UI visibility
    bool keyedOn(int ch) const { return rt_[size_t(ch)].keyOn; }

private:
    enum Stage : uint8_t { kOff, kAttack, kDecay, kSustain, kRelease };
    struct OpRt {
        double phase = 0, inc = 0;
        int env = 1023, heldAtt = 1023;
        uint32_t frac = 0, attFrac = 0;
        uint8_t stage = kOff, inv = 0, held = 0, ssgActive = 0;
    };
    struct ChRt {
        std::array<OpRt, kYmOps> op{};
        bool keyOn = false, sustainHeld = false, pendingOff = false, dacPlaying = false;
        int note = 60, vel = 100, ccVol = 100, ccExpr = 127, ccPan = 64, ccMod = 0, ccBright = 64, ccRes = 64;
        float sendRev = 0, sendCho = 0, sendDly = 0;
        float bend = 0, fb1 = 0, fb2 = 0, gl = 1, gr = 1;
        double dacPos = 0, dacStep = 0;
    };

    template <class V> void visit(V& v);
    void tickEg();
    void tickOp(OpRt& o, const YmOperatorParams& p, int kc);
    void updateIncs(int ch);
    void recomputePan(int ch);
    void keyOn(int ch);
    void keyOff(int ch);
    void renderChannel(int ch, float& l, float& r);
    int gp(YmParam p) const { return g_[size_t(p)]; }

    double sr_ = 44100;
    std::array<int, 8> g_{};
    std::array<YmChannelParams, kYmChannels> ch_{};
    std::array<ChRt, kYmChannels> rt_{};
    double lfoPhase_ = 0, egAcc_ = 0;
    std::array<float, 1024> gainTab_{};
    const uint8_t* dac_ = nullptr;
    size_t dacLen_ = 0;
    double dacRate_ = 8000;
    FxRack fx_;
};

}  // namespace aur::audio
