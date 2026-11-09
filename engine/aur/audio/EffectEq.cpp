// EffectEq.cpp - 3-band shelving/peak EQ and a state-variable filter.
#include "aur/audio/Effect.hpp"

#include <cmath>

namespace aur::audio
{
    namespace
    {

        class EqThreeBand : public ParamBase
        {
        public:
            enum Param
            {
                LowGain,
                MidGain,
                HighGain,
                MidFreq,
                MidQ,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                recount();
            }
            // Param changes re-derive the coefficients (template method calls
            // this after values_ is updated).
            void paramsChanged() override { recount(); }
            void reset() override { s1l_ = s2l_ = s1r_ = s2r_ = 0; }
            void processImpl(float *l, float *r, size_t n) override
            {
                for (size_t i = 0; i < n; ++i)
                {
                    l[i] = band(l[i], s1l_, s2l_);
                    r[i] = band(r[i], s1r_, s2r_);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "3-Band EQ"; }

        private:
            static const EffectParamInfo kTable[Count];
            void recount()
            {
                const double f0 = 40.0; // low shelf corner
                const double fm = std::clamp(double(values_[MidFreq]), 100.0, 5000.0);
                const double q = std::max(0.5, values_[MidQ] / 10.0);
                const double w0 = 2.0 * 3.14159265358979323846 * fm / rate_;
                mid_ = std::tan(w0 / 2.0) / q; // gain of the mid band-pass (bilinear)
                lowK_ = std::exp(-2.0 * 3.14159265358979323846 * f0 / rate_);
                highK_ = 0.5; // fixed blend for the high band (x - lp above)
                lowG_ = dbToLin(values_[LowGain]);
                midG_ = dbToLin(values_[MidGain]);
                highG_ = dbToLin(values_[HighGain]);
            }
            static double dbToLin(int db10) { return std::exp2(double(db10) / 60.207); }
            float band(float x, double &s1, double &s2)
            {
                // low shelf (1-pole LP blend) + mid peak (biquad-ish BP mix) + high shelf (1-pole HP blend)
                const double lp = s1 + lowK_ * (double(x) - s1);
                s1 = lp;
                const double hp = double(x) - lp;
                const double bp = s2 + mid_ * (hp - s2);
                s2 = bp;
                const double out = lp * lowG_ + bp * midG_ * 0.5 + hp * highG_;
                return float(out);
            }
            double rate_ = 44100.0, lowK_ = 0.5, highK_ = 0.5, mid_ = 1.0;
            double lowG_ = 1.0, midG_ = 1.0, highG_ = 1.0;
            double s1l_ = 0, s2l_ = 0, s1r_ = 0, s2r_ = 0;
        };

        const EffectParamInfo EqThreeBand::kTable[EqThreeBand::Count] = {
            {"Low Gain (dB/10)", -120, 120, 0}, {"Mid Gain (dB/10)", -120, 120, 0}, {"High Gain (dB/10)", -120, 120, 0}, {"Mid Freq (Hz)", 100, 5000, 1000}, {"Mid Q (0.1x)", 5, 100, 10}};

        class StateVariableFilter : public ParamBase
        {
        public:
            enum Param
            {
                Mode,
                Cutoff,
                Resonance,
                Count
            }; // Mode: 0 LP, 1 BP, 2 HP
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                recompute();
            }
            void paramsChanged() override { recompute(); }
            void reset() override { low_ = band_ = lowR_ = bandR_ = 0; }
            void processImpl(float *l, float *r, size_t n) override
            {
                for (size_t i = 0; i < n; ++i)
                {
                    l[i] = step(l[i], low_, band_);
                    r[i] = step(r[i], lowR_, bandR_);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Filter"; }

        private:
            static const EffectParamInfo kTable[Count];
            void recompute() { f_ = 2.0 * std::sin(3.14159265358979323846 * std::clamp(values_[Cutoff], 20, 20000) / rate_); }
            float step(float x, double &low, double &band)
            {
                const double q = 1.0 - std::clamp(values_[Resonance], 0, 95) / 100.0;
                low += f_ * band;
                const double high = double(x) - low - q * band;
                band += f_ * high;
                const int mode = values_[Mode];
                return float(mode == 0 ? low : mode == 1 ? band
                                                         : high);
            }
            double rate_ = 44100.0, f_ = 0.1, low_ = 0, band_ = 0, lowR_ = 0, bandR_ = 0;
        };

        const EffectParamInfo StateVariableFilter::kTable[StateVariableFilter::Count] = {
            {"Mode (LP/BP/HP)", 0, 2, 0}, {"Cutoff (Hz)", 20, 20000, 2000}, {"Resonance (%)", 0, 95, 10}};

    } // namespace

    namespace
    {

        class Compressor : public ParamBase
        {
        public:
            enum Param
            {
                Threshold,
                Ratio,
                AttackMs,
                ReleaseMs,
                Makeup,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                env_ = 0;
                gain_ = 1;
            }
            void reset() override
            {
                env_ = 0;
                gain_ = 1;
            }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double at = std::exp(-1.0 / (std::max(1, values_[AttackMs]) * 0.001 * rate_));
                const double rl = std::exp(-1.0 / (std::max(1, values_[ReleaseMs]) * 0.001 * rate_));
                const double th = std::exp2(double(values_[Threshold]) / 100.0 / 6.0);
                const double slope = 1.0 - 1.0 / std::max(1, values_[Ratio]);
                const double makeup = std::exp2(double(values_[Makeup]) / 100.0 / 6.0);
                for (size_t i = 0; i < n; ++i)
                {
                    const double lvl = std::max(std::fabs(double(l[i])), std::fabs(double(r[i])));
                    env_ = lvl > env_ ? at * env_ + (1 - at) * lvl : rl * env_ + (1 - rl) * lvl;
                    double target = 1.0;
                    if (env_ > th)
                        target = std::exp2(std::log2(env_ / th) * -slope);
                    gain_ += (target - gain_) * 0.5;
                    l[i] = float(l[i] * gain_ * makeup);
                    r[i] = float(r[i] * gain_ * makeup);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Compressor"; }

        private:
            static const EffectParamInfo kTable[Count];
            double rate_ = 44100.0, env_ = 0, gain_ = 1;
        };

        const EffectParamInfo Compressor::kTable[Compressor::Count] = {
            {"Threshold (dB/100)", -600, 0, -200}, {"Ratio (x)", 1, 20, 4}, {"Attack (ms)", 1, 100, 10}, {"Release (ms)", 1, 1000, 100}, {"Makeup (dB/100)", 0, 240, 0}};

        class Limiter : public ParamBase
        {
        public:
            enum Param
            {
                Ceiling,
                ReleaseMs,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                gain_ = 1;
            }
            void reset() override { gain_ = 1; }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double rl = std::exp(-1.0 / (std::max(1, values_[ReleaseMs]) * 0.001 * rate_));
                const double ceilLin = std::exp2(double(values_[Ceiling]) / 100.0 / 6.0);
                for (size_t i = 0; i < n; ++i)
                {
                    const double lvl = std::max(std::fabs(double(l[i])), std::fabs(double(r[i])));
                    const double want = lvl > ceilLin ? ceilLin / lvl : 1.0;
                    gain_ = std::min(want, rl * gain_ + (1 - rl) * want);
                    l[i] = float(l[i] * gain_);
                    r[i] = float(r[i] * gain_);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Limiter"; }

        private:
            static const EffectParamInfo kTable[Count];
            double rate_ = 44100.0, gain_ = 1;
        };

        const EffectParamInfo Limiter::kTable[Limiter::Count] = {
            {"Ceiling (dB/100)", -1200, 0, -10}, {"Release (ms)", 1, 1000, 50}};

    } // namespace
} // namespace aur::audio

namespace aur::audio {

std::unique_ptr<IEffect> spawnEqThreeBand() { return std::make_unique<EqThreeBand>(); }
std::unique_ptr<IEffect> spawnStateVariableFilter() { return std::make_unique<StateVariableFilter>(); }
std::unique_ptr<IEffect> spawnCompressor() { return std::make_unique<Compressor>(); }
std::unique_ptr<IEffect> spawnLimiter() { return std::make_unique<Limiter>(); }

} // namespace aur::audio
