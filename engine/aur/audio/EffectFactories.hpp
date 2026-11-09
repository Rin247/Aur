// EffectFactories.hpp - internal spawn hooks for the builtin effect leaves.
// Each Effect*.cpp defines these for its classes; EffectRegistry.cpp binds
// them into the id table. Not part of the public engine API.
#pragma once
#include <memory>
#include "aur/audio/Effect.hpp"

namespace aur::audio {
std::unique_ptr<IEffect> spawnEqThreeBand();
std::unique_ptr<IEffect> spawnStateVariableFilter();
std::unique_ptr<IEffect> spawnCompressor();
std::unique_ptr<IEffect> spawnLimiter();
std::unique_ptr<IEffect> spawnDistortion();
std::unique_ptr<IEffect> spawnBitcrusher();
std::unique_ptr<IEffect> spawnTremolo();
std::unique_ptr<IEffect> spawnWidener();
std::unique_ptr<IEffect> spawnPhaser();
std::unique_ptr<IEffect> spawnSnesEchoFir();
} // namespace aur::audio
