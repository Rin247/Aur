// EffectEchoFir.cpp - SNES echo FIR builtin wrapping SnesEcho.
#include "aur/audio/Effect.hpp"
#include "aur/audio/SnesEcho.hpp"

namespace aur::audio
{
    namespace
    {

        class SnesEchoFir : public ParamBase
        {
        public:
            enum Param
            {
                DelayMs,
                Feedback,
                WetPct,
                WriteEnable,
                Count
            };
            void init(double sr) override { ensureDefaults(); echo_.init(sr); }
            void reset() override { echo_.reset(); }
            void processImpl(float *l, float *r, size_t n) override
            {
                // Wet-only output: the builtin returns the FIR echo alone; the
                // chain adds the dry signal separately.
                const double wet = double(values_[WetPct]) / 100.0;
                echo_.setDelayMs(float(values_[DelayMs]));
                echo_.setFeedback(float(values_[Feedback]) / 100.0f);
                echo_.setBufferWrite(values_[WriteEnable] != 0);
                for (size_t i = 0; i < n; ++i)
                {
                    float wetL = 0, wetR = 0;
                    echo_.process(l[i], r[i], wetL, wetR);
                    l[i] = float(wetL * wet);
                    r[i] = float(wetR * wet);
                }
            }
            const EffectParamInfo *paramTable() const override { return kTable; }
            int paramCount() const override { return Count; }
            const char *name() const override { return "SNES Echo FIR"; }

        private:
            static const EffectParamInfo kTable[Count];
            SnesEcho echo_;
        };

        const EffectParamInfo SnesEchoFir::kTable[SnesEchoFir::Count] = {
            {"Echo Delay (ms)", 0, 240, 80}, {"Feedback (%)", -100, 100, 40},
            {"Wet (%)", 0, 100, 100}, {"Buffer Write", 0, 1, 1}};

    } // namespace
} // namespace aur::audio

namespace aur::audio {

std::unique_ptr<IEffect> spawnSnesEchoFir() { return std::make_unique<SnesEchoFir>(); }

} // namespace aur::audio
