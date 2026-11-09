// EffectChain.cpp - master chain runner.
#include "aur/audio/EffectChain.hpp"

#include <algorithm>
#include <cmath>

namespace aur::audio
{

    bool EffectChain::build(double sampleRate, const std::vector<Slot> &slots)
    {
        slots_ = slots;
        fx_.clear();
        fx_.reserve(slots_.size());
        for (const Slot &s : slots_)
        {
            auto fx = makeEffect(s.id);
            if (!fx)
            {
                fx_.clear();
                slots_.clear();
                return false;
            }
            fx->init(sampleRate);
            fx_.push_back(std::move(fx));
        }
        // A chain of only bypassed slots is the same as no chain: mark it empty
        // so callers keep the exact unprocessed path (bit-exact bypass).
        if (std::all_of(slots_.begin(), slots_.end(),
                        [](const Slot &s) { return s.bypass; }))
        {
            fx_.clear();
            slots_.clear();
        }
        return true;
    }

    void EffectChain::reset()
    {
        for (auto &fx : fx_)
            fx->reset();
    }

    void EffectChain::process(float *left, float *right, size_t frames,
                              float &peakOutL, float &peakOutR)
    {
        for (size_t i = 0; i < fx_.size(); ++i)
        {
            if (slots_[i].bypass)
                continue;
            fx_[i]->process(left, right, frames);
        }
        float pl = 0.f, pr = 0.f;
        for (size_t i = 0; i < frames; ++i)
        {
            pl = std::max(pl, std::fabs(left[i]));
            pr = std::max(pr, std::fabs(right[i]));
        }
        peakOutL = pl;
        peakOutR = pr;
        budget_ += std::int64_t(frames) * std::int64_t(fx_.size());
    }

    void EffectChain::processInterleaved(float *data, size_t frames, float *scratchL,
                                         float *scratchR, float &peakOutL, float &peakOutR)
    {
        if (empty())
        {
            // Still report peaks so meters work on the bypassed path.
            float pl = 0.f, pr = 0.f;
            for (size_t i = 0; i < frames; ++i)
            {
                pl = std::max(pl, std::fabs(data[2 * i]));
                pr = std::max(pr, std::fabs(data[2 * i + 1]));
            }
            peakOutL = pl;
            peakOutR = pr;
            return;
        }
        for (size_t i = 0; i < frames; ++i)
        {
            scratchL[i] = data[2 * i];
            scratchR[i] = data[2 * i + 1];
        }
        process(scratchL, scratchR, frames, peakOutL, peakOutR);
        for (size_t i = 0; i < frames; ++i)
        {
            data[2 * i] = scratchL[i];
            data[2 * i + 1] = scratchR[i];
        }
    }

} // namespace aur::audio
