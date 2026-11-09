// YmFmPatches.hpp - 13 original built-in patches (no values copied from any game or bank).
#pragma once
#include "YmFmCore.hpp"

namespace aur::audio {

inline constexpr int kYmBuiltinPatchCount = 13;
struct YmBuiltinPatch { const char* name; YmChannelParams params; };
const YmBuiltinPatch& ymBuiltinPatch(int index);  // index clamped to [0, count)

}  // namespace aur::audio
