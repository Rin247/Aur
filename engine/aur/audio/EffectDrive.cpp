// EffectDrive.cpp - distortion, bitcrusher, tremolo, widener builtins.
#include "aur/audio/Effect.hpp"

#include <cmath>

namespace aur::audio
{
    namespace
    {

        class Distortion : public ParamBase
        {
        public:
            enum Param
            {
                Drive,
                Tone,
                Level,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                lp_ = 0;
            }
            void reset() override { lp_ = 0; }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double drive = std::exp2(double(values_[Drive]) / 100.0);
                const double level = std::exp2(double(values_[Level]) / 100.0 / 6.0);
                const double k = std::clamp(values_[Tone], 0, 100) / 100.0;
                for (size_t i = 0; i < n; ++i)
                {
                    l[i] = shape(l[i] * float(drive), k) * float(level);
                    r[i] = shape(r[i] * float(drive), k) * float(level);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Distortion"; }

        private:
            static const EffectParamInfo kTable[Count];
            static float shape(float x, double k)
            {
                const double t = std::tanh(double(x));
                return float(t * (1.0 - k) + std::clamp(t * 1.5, -1.0, 1.0) * k);
            }
            double rate_ = 44100.0, lp_ = 0;
        };

        const EffectParamInfo Distortion::kTable[Distortion::Count] = {
            {"Drive (0.5x)", 0, 1200, 40}, {"Tone (%)", 0, 100, 50}, {"Level (dB/100)", -600, 0, -60}};

        class Bitcrusher : public ParamBase
        {
        public:
            enum Param
            {
                Bits,
                Downsample,
                Mix,
                Count
            };
            void init(double) override { ensureDefaults(); }
            void reset() override
            {
                holdL_ = holdR_ = 0;
                count_ = 0;
            }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double levels = std::exp2(double(std::max(1, values_[Bits])));
                const int decim = std::max(1, values_[Downsample]);
                const double mix = double(values_[Mix]) / 100.0;
                for (size_t i = 0; i < n; ++i)
                {
                    if (count_ % decim == 0)
                    {
                        holdL_ = std::round(double(l[i]) * levels) / levels;
                        holdR_ = std::round(double(r[i]) * levels) / levels;
                    }
                    ++count_;
                    l[i] = float(holdL_ * mix + double(l[i]) * (1.0 - mix));
                    r[i] = float(holdR_ * mix + double(r[i]) * (1.0 - mix));
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Bitcrusher"; }

        private:
            static const EffectParamInfo kTable[Count];
            double holdL_ = 0, holdR_ = 0;
            long long count_ = 0;
        };

        const EffectParamInfo Bitcrusher::kTable[Bitcrusher::Count] = {
            {"Bits", 1, 16, 8}, {"Downsample (x)", 1, 32, 1}, {"Mix (%)", 0, 100, 100}};

        class Tremolo : public ParamBase
        {
        public:
            enum Param
            {
                RateHz,
                Depth,
                StereoPhase,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                phaseL_ = 0;
            }
            void reset() override { phaseL_ = 0; }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double inc = 2.0 * 3.14159265358979323846 * std::clamp(values_[RateHz], 1, 100) / rate_;
                const double depth = double(values_[Depth]) / 100.0;
                const double ph = double(values_[StereoPhase]) / 360.0;
                for (size_t i = 0; i < n; ++i)
                {
                    const double gl = 1.0 - depth * 0.5 * (1.0 - std::cos(phaseL_));
                    const double gr = 1.0 - depth * 0.5 * (1.0 - std::cos(phaseL_ + ph * 2.0 * 3.14159265358979323846));
                    phaseL_ += inc;
                    if (phaseL_ > 2.0 * 3.14159265358979323846)
                        phaseL_ -= 2.0 * 3.14159265358979323846;
                    l[i] = float(l[i] * gl);
                    r[i] = float(r[i] * gr);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Tremolo"; }

        private:
            static const EffectParamInfo kTable[Count];
            double rate_ = 44100.0, phaseL_ = 0;
        };

        const EffectParamInfo Tremolo::kTable[Tremolo::Count] = {
            {"Rate (Hz)", 1, 100, 50}, {"Depth (%)", 0, 100, 80}, {"Stereo Phase (deg)", 0, 360, 0}};

        class Widener : public ParamBase
        {
        public:
            enum Param
            {
                Width,
                HaasMs,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                delayL_ = delayR_ = 0;
            }
            void reset() override { delayL_ = delayR_ = 0; }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double width = double(values_[Width]) / 100.0;
                const int haas = int(std::clamp(values_[HaasMs], 0, 20) * 0.001 * rate_);
                for (size_t i = 0; i < n; ++i)
                {
                    const double mid = (double(l[i]) + double(r[i])) * 0.5;
                    const double side = (double(l[i]) - double(r[i])) * 0.5;
                    double hl = 0, hr = 0;
                    if (haas > 0)
                    {
                        hl = tap(lBuf_, head_, haas);
                        hr = tap(rBuf_, head_, haas);
                        push(lBuf_, head_, double(l[i]));
                        push(rBuf_, head_, double(r[i]));
                    }
                    l[i] = float(std::clamp(mid - side * width + hl * 0.3, -2.0, 2.0));
                    r[i] = float(std::clamp(mid + side * width + hr * 0.3, -2.0, 2.0));
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Widener"; }

        private:
            static const EffectParamInfo kTable[Count];
            static void push(double (&buf)[4096], int &head, double v)
            {
                buf[head] = v;
                head = (head + 1) % 4096;
            }
            static double tap(const double (&buf)[4096], int head, int d)
            {
                int idx = head - d;
                if (idx < 0)
                    idx += 4096;
                return buf[idx];
            }
            double rate_ = 44100.0, delayL_ = 0, delayR_ = 0;
            double lBuf_[4096] = {}, rBuf_[4096] = {};
            int head_ = 0;
        };

        const EffectParamInfo Widener::kTable[Widener::Count] = {
            {"Width (%)", 0, 200, 120}, {"Haas (0.1 ms)", 0, 200, 0}};

    } // namespace

    std::unique_ptr<IEffect> spawnDistortion() { return std::make_unique<Distortion>(); }
    std::unique_ptr<IEffect> spawnBitcrusher() { return std::make_unique<Bitcrusher>(); }
    std::unique_ptr<IEffect> spawnTremolo() { return std::make_unique<Tremolo>(); }
    std::unique_ptr<IEffect> spawnWidener() { return std::make_unique<Widener>(); }

} // namespace aur::audio
