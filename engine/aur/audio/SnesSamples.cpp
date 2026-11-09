// SnesSamples.cpp - SnesPcmCore sample pool: PCM16/BRR loading, procedural demo samples.
#include <algorithm>
#include <cmath>

#include "BrrCodec.hpp"
#include "SnesPcmCore.hpp"

namespace aur::audio {
namespace {
constexpr double kTwoPi = 6.283185307179586;
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

// ------------------------------------------------------------------ samples
void SnesPcmCore::clearSamples() { nSlots_ = 0; poolUsed_ = 0; for (auto& s : slots_) s = Slot{}; }
void SnesPcmCore::setGaussTable(const int16_t t[512]) { std::copy(t, t + 512, gauss_.begin()); }

int SnesPcmCore::storeSlot(size_t frames, double rate, int root, bool loop, size_t loopStart) {
    if (nSlots_ >= kSnesMaxSlots || frames == 0) return -1;
    Slot& s = slots_[size_t(nSlots_)];
    s = Slot{poolUsed_, frames, std::min(loopStart, frames - 1), rate, clampi(root, 0, 127), loop};
    poolUsed_ += frames;
    return nSlots_++;
}

int SnesPcmCore::loadPcm16(const int16_t* d, size_t n, double rate, int root, bool loop, size_t loopStart) {
    if (!d || n == 0 || poolUsed_ + n > pool_.size()) return -1;
    std::copy(d, d + n, pool_.begin() + long(poolUsed_));
    return storeSlot(n, rate, root, loop, loopStart);
}

int SnesPcmCore::loadBrr(const uint8_t* d, size_t blocks, double rate, int root, int loopBlock) {
    if (!d || blocks == 0 || poolUsed_ >= pool_.size()) return -1;
    const BrrDecodeInfo info = brrDecode(d, blocks, pool_.data() + poolUsed_, pool_.size() - poolUsed_);
    if (info.samples == 0) return -1;
    const bool loop = loopBlock >= 0 || (info.endFlag && info.loopFlag);
    return storeSlot(info.samples, rate, root, loop, size_t(std::max(0, loopBlock)) * 16);
}

int SnesPcmCore::loadDemoSamples() {
    auto encodeLoad = [&](const std::vector<int16_t>& pcm, double rate, bool loop) {
        std::vector<uint8_t> bytes(brrEncodedSize(pcm.size()));
        BrrEncodeOptions o; o.loop = loop;
        brrEncode(pcm.data(), pcm.size(), o, bytes.data(), bytes.size());
        return loadBrr(bytes.data(), bytes.size() / kBrrBytesPerBlock, rate, 60, loop ? 0 : -1);
    };
    const double cycleRate = 32.0 * 261.6255653;  // one 32-sample cycle plays at middle C when note == root
    std::vector<int16_t> saw(32), pulse(32), snare(2400);
    for (int i = 0; i < 32; ++i) {
        double s = 0;
        for (int h = 1; h <= 10; ++h) s += std::sin(kTwoPi * h * i / 32.0) / h;
        saw[size_t(i)] = int16_t(std::lround(s * 9000.0));
        pulse[size_t(i)] = int16_t(i < 8 ? 9000 : -9000);
    }
    uint32_t rng = 0x2545F491u;  // fixed seed: demo noise is identical on every run
    for (int i = 0; i < 2400; ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        const double n = double(int32_t(rng)) / 2147483648.0;
        const double tone = std::sin(kTwoPi * 185.0 * i / 16000.0);
        snare[size_t(i)] = int16_t(std::lround((0.7 * n + 0.4 * tone) * std::exp(-i / 500.0) * 20000.0));
    }
    const int first = encodeLoad(saw, cycleRate, true);
    if (first < 0) return -1;
    if (encodeLoad(pulse, cycleRate, true) < 0 || encodeLoad(snare, 16000.0, false) < 0) return -1;
    return first;
}

}  // namespace aur::audio
