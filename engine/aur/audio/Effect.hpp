// Effect.hpp - IEffect framework + builtin registry for the master chain.
// Lifecycle: init() once (may allocate), everything after is allocation-free.
// Parameters follow the k*ParamTable discipline used by FxRack and the cores.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace aur::audio
{

    struct EffectParamInfo
    {
        const char *name;
        int min;
        int max;
        int def;
    };

    enum class EffectId : uint8_t
    {
        EqThreeBand,
        StateVariableFilter,
        Compressor,
        Limiter,
        Distortion,
        Bitcrusher,
        Tremolo,
        Widener,
        Phaser,
        SnesEchoFir,
        Count
    };

    class IEffect
    {
    public:
        virtual ~IEffect() = default;
        virtual void init(double sampleRate) = 0;
        virtual void reset() = 0;
        // In-place stereo processing; allocation-free.
        virtual void process(float *left, float *right, size_t frames) = 0;
        // Clamped setter; returns false when the id is out of range.
        virtual bool setParam(int id, int value) = 0;
        virtual int param(int id) const = 0;
        virtual const EffectParamInfo *paramTable() const = 0;
        virtual int paramCount() const = 0;
        virtual const char *name() const = 0;
        // Tempo hint (bpm) for syncable params; called at song swap only.
        virtual void tempo(double) {}
    };

    std::unique_ptr<IEffect> makeEffect(EffectId id);
    const char *effectName(EffectId id);

    // Builds a chain from a song's SongDocument::EffectSlot list (type strings).
    // Unknown types are skipped; returns false if the result is empty but the
    // input was not (nothing valid to build).
    bool buildChainFromTypes(double sampleRate,
                             const std::vector<std::pair<std::string, bool>> &slots,
                             const std::vector<std::vector<std::pair<int, int>>> &params,
                             class EffectChain &out);

    // Clamped-parameter base: owns the value vector; effects own only their
    // DSP and tables. paramTable()/paramCount() are implemented by the leaf.
    // Defaults load lazily on first access so leaves stay default-constructible
    // (the registry uses make_unique<T>()).
    class ParamBase : public IEffect {
    public:
        // Non-virtual template method: guarantees defaults are loaded before
        // the leaf DSP runs, so a leaf cannot read an empty values_ vector.
        void process(float *left, float *right, size_t frames) final {
            ensureDefaults();
            processImpl(left, right, frames);
        }

        bool setParam(int id, int value) override {
            if (id < 0 || id >= paramCount()) return false;
            ensureDefaults();
            const EffectParamInfo &t = paramTable()[id];
            values_[size_t(id)] = std::clamp(value, t.min, t.max);
            paramsChanged();
            return true;
        }
        int param(int id) const override {
            const_cast<ParamBase *>(this)->ensureDefaults();
            return values_[size_t(id)];
        }

    protected:
        void ensureDefaults() {
            if (values_.size() == size_t(paramCount())) return;
            values_.resize(size_t(paramCount()));
            for (int i = 0; i < paramCount(); ++i)
                values_[size_t(i)] = paramTable()[i].def;
        }
        // Leaf DSP; call after defaults are guaranteed.
        virtual void processImpl(float *left, float *right, size_t frames) = 0;
        // Called after any parameter change; default is no-op.
        virtual void paramsChanged() {}
        std::vector<int> values_;
    };

} // namespace aur::audio
