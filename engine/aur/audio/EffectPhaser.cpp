// EffectPhaser.cpp - 4-stage all-pass phaser and EffectEchoFir.cpp's SNES FIR.
#include "aur/audio/Effect.hpp"
#include "aur/audio/SnesEcho.hpp"

#include <cmath>

namespace aur::audio
{
    namespace
    {

        class Phaser : public ParamBase
        {
        public:
            enum Param
            {
                RateHz,
                Depth,
                Feedback,
                Stages,
                Count
            };
            void init(double sr) override
            {
                ensureDefaults();
                rate_ = sr;
                phase_ = 0;
                z_[0] = z_[1] = 0;
            }
            void reset() override
            {
                phase_ = 0;
                for (auto &st : ap_)
                    st[0] = st[1] = 0;
                z_[0] = z_[1] = 0;
            }
            void processImpl(float *l, float *r, size_t n) override
            {
                const double inc = 2.0 * 3.14159265358979323846 * std::clamp(values_[RateHz], 1, 100) / rate_;
                const double depth = double(values_[Depth]) / 100.0;
                const double fb = double(values_[Feedback]) / 100.0;
                const int stages = std::clamp(values_[Stages], 1, 4);
                for (size_t i = 0; i < n; ++i)
                {
                    const double sweep = 0.5 + 0.5 * std::sin(phase_);
                    phase_ += inc;
                    if (phase_ > 2.0 * 3.14159265358979323846)
                        phase_ -= 2.0 * 3.14159265358979323846;
                    double xl = double(l[i]) + fb * z_[0];
                    double xr = double(r[i]) + fb * z_[1];
                    for (int s = 0; s < stages; ++s)
                    {
                        xl = ap(xl, g(sweep, depth), ap_[s][0]);
                        xr = ap(xr, g(sweep, depth), ap_[s][1]);
                    }
                    z_[0] = xl - double(l[i]);
                    z_[1] = xr - double(r[i]);
                    l[i] = float(xl);
                    r[i] = float(xr);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "Phaser"; }

        private:
            static const EffectParamInfo kTable[Count];
            static double ap(double x, double g, double &state)
            {
                // one-multiply all-pass: y[n] = g*(x[n] - y[n-1]) + x[n-1]
                const double out = g * (x - state) + state;
                state = x;
                return out;
            }
            static double g(double sweep, double depth) { return 0.5 + 0.49 * sweep * depth; }
            double rate_ = 44100.0, phase_ = 0, ap_[4][2] = {}, z_[2] = {};
        };

        const EffectParamInfo Phaser::kTable[Phaser::Count] = {
            {"Rate (Hz)", 1, 100, 20}, {"Depth (%)", 0, 100, 70}, {"Feedback (%)", -90, 90, 30}, {"Stages", 1, 4, 4}};

    } // namespace
} // namespace aur::audio

namespace aur::audio {

std::unique_ptr<IEffect> spawnPhaser() { return std::make_unique<Phaser>(); }

} // namespace aur::audio
