#pragma once

// Sf2Fixture.hpp - a tiny SoundFont bank generated in code, for tests only.
// Real banks are tens of MB and carry an unverified licence (P7a contract 2),
// so the tests run against this bank: one preset, one looped 440 Hz sine.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aur::testsupport
{
    // Returns a complete SF2 file image as a byte vector.
    std::vector<std::uint8_t> make_test_bank();

    // The bank's one preset: GM program 0, bank 0.
    inline constexpr int kTestPresetProgram = 0;
    // The pitch the bank's sample is tuned to.
    inline constexpr int kTestPresetRootKey = 60; // middle C
    // Frames in the generated sine sample.
    inline constexpr int kTestSampleFrames = 2048;
    // Sample rate the generated sample declares.
    inline constexpr int kTestSampleRate = 44100;
} // namespace aur::testsupport
