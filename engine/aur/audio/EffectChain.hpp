// EffectChain.hpp - ordered master effect chain with dry path, meters and a
// CPU-budget counter. Built from the song's "effects" key; empty = old path.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "aur/audio/Effect.hpp"

namespace aur::audio
{

    class EffectChain
    {
    public:
        struct Slot
        {
            EffectId id = EffectId::EqThreeBand;
            bool bypass = false;
        };

        // Builds instances; call off the audio thread. Returns false and clears
        // on an unknown effect id.
        bool build(double sampleRate, const std::vector<Slot> &slots);
        void reset();

        // In-place stereo processing over the active chain (bypassed slots skip).
        // Allocation-free. Returns the post-chain peak levels via the out params.
        void process(float *left, float *right, size_t frames, float &peakOutL, float &peakOutR);

        // Interleaved variant: deinterleaves into caller scratch (capacity >=
        // frames), runs the chain, interleaves back. Allocation-free.
        void processInterleaved(float *data, size_t frames, float *scratchL, float *scratchR,
                                float &peakOutL, float &peakOutR);

        bool empty() const { return slots_.empty(); }
        size_t size() const { return slots_.size(); }
        IEffect *effect(size_t index) const { return fx_[index].get(); }
        bool bypassed(size_t index) const { return slots_[index].bypass; }

        // Rough CPU cost: samples processed since the last accounting reset.
        void resetBudget() { budget_ = 0; }
        std::int64_t budget() const { return budget_; }

    private:
        std::vector<Slot> slots_;
        std::vector<std::unique_ptr<IEffect>> fx_;
        std::int64_t budget_ = 0;
    };

} // namespace aur::audio
