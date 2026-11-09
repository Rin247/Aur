// BrrCodec.hpp - SNES BRR (Bit Rate Reduction) block codec. 9-byte blocks -> 16 samples.
// Header byte: bits 7-4 range (shift), bits 3-2 filter, bit 1 loop, bit 0 end.
#pragma once
#include <cstddef>
#include <cstdint>

namespace aur::audio {

inline constexpr int kBrrSamplesPerBlock = 16;
inline constexpr int kBrrBytesPerBlock = 9;

struct BrrFilterState { int p1 = 0, p2 = 0; };  // previous two outputs, 15-bit domain

struct BrrDecodeInfo {
    size_t samples = 0;     // PCM16 samples written
    size_t blocks = 0;      // blocks consumed
    bool endFlag = false;   // an end block was reached
    bool loopFlag = false;  // that end block had the loop bit
};

// Decode one nibble (-8..7). Returns the 16-bit output sample and advances the filter state.
int brrDecodeNibble(int nibble, int range, int filter, BrrFilterState& st);

// Decode one 9-byte block into 16 samples; returns the block's header flags (bit0 end, bit1 loop).
int brrDecodeBlock(const uint8_t* block, BrrFilterState& st, int16_t out[kBrrSamplesPerBlock]);

// Decode up to `blocks` blocks (stops after an end block or when outCapacity is reached).
BrrDecodeInfo brrDecode(const uint8_t* data, size_t blocks, int16_t* out, size_t outCapacity);

struct BrrEncodeOptions {
    bool loop = false;            // set loop bit on the final block
    size_t loopStartSample = 0;   // snapped down to a block; that block is forced to filter 0
};
size_t brrEncodedSize(size_t sampleCount);  // bytes needed (always >= 9)

// Brute-force best filter/range per block (minimum squared error on the real decoder path).
// Returns bytes written, or 0 if outCapacity is too small.
size_t brrEncode(const int16_t* pcm, size_t count, const BrrEncodeOptions& opt, uint8_t* out, size_t outCapacity);

}  // namespace aur::audio
