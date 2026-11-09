// EffectRegistry.cpp - effect id -> instance factory. One row per builtin;
// adding an effect touches this table only.
#include "aur/audio/EffectFactories.hpp"
#include "aur/audio/EffectChain.hpp"

#include <array>
#include <string>

namespace aur::audio
{
    namespace
    {

        struct Entry
        {
            EffectId id;
            const char *name;
            std::unique_ptr<IEffect> (*make)();
        };

        constexpr std::array<Entry, size_t(EffectId::Count)> kEntries{{
            {EffectId::EqThreeBand, "3-Band EQ", &spawnEqThreeBand},
            {EffectId::StateVariableFilter, "Filter", &spawnStateVariableFilter},
            {EffectId::Compressor, "Compressor", &spawnCompressor},
            {EffectId::Limiter, "Limiter", &spawnLimiter},
            {EffectId::Distortion, "Distortion", &spawnDistortion},
            {EffectId::Bitcrusher, "Bitcrusher", &spawnBitcrusher},
            {EffectId::Tremolo, "Tremolo", &spawnTremolo},
            {EffectId::Widener, "Widener", &spawnWidener},
            {EffectId::Phaser, "Phaser", &spawnPhaser},
            {EffectId::SnesEchoFir, "SNES Echo FIR", &spawnSnesEchoFir},
        }};

    } // namespace

    const char *effectName(EffectId id)
    {
        const size_t i = size_t(id);
        return i < kEntries.size() ? kEntries[i].name : "Unknown";
    }

    std::unique_ptr<IEffect> makeEffect(EffectId id)
    {
        const size_t i = size_t(id);
        return i < kEntries.size() ? kEntries[i].make() : nullptr;
    }

    bool buildChainFromTypes(double sampleRate,
                             const std::vector<std::pair<std::string, bool>> &slots,
                             const std::vector<std::vector<std::pair<int, int>>> &params,
                             EffectChain &out)
    {
        std::vector<EffectChain::Slot> built;
        std::vector<std::vector<std::pair<int, int>>> builtParams;
        for (size_t i = 0; i < slots.size(); ++i)
        {
            for (const Entry &e : kEntries)
            {
                if (slots[i].first == e.name)
                {
                    built.push_back({e.id, slots[i].second});
                    builtParams.push_back(i < params.size() ? params[i]
                                                            : std::vector<std::pair<int, int>>{});
                    break;
                }
            }
        }
        if (built.empty()) return slots.empty();
        if (!out.build(sampleRate, built)) return false;
        for (size_t i = 0; i < built.size(); ++i)
            for (const auto &pv : builtParams[i])
                out.effect(i)->setParam(pv.first, pv.second);
        return true;
    }

} // namespace aur::audio
