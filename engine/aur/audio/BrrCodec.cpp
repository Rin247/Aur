#include "BrrCodec.hpp"

#include <algorithm>

namespace aur::audio {

int brrDecodeNibble(int n, int range, int filter, BrrFilterState& st) {
    int s = range <= 12 ? (n << range) >> 1 : (n < 0 ? -2048 : 0);
    switch (filter & 3) {  // ~15/16, 61/32 & 15/16, 115/64 & 13/16 recursive predictors
    case 1: s += st.p1 - (st.p1 >> 4); break;
    case 2: s += 2 * st.p1 - ((3 * st.p1) >> 5) - st.p2 + (st.p2 >> 4); break;
    case 3: s += 2 * st.p1 - ((13 * st.p1) >> 6) - st.p2 + ((3 * st.p2) >> 4); break;
    default: break;
    }
    s = std::clamp(s, -32768, 32767);
    const int16_t wrapped = int16_t(uint16_t(s) << 1);  // 15-bit wrap, output is doubled
    st.p2 = st.p1;
    st.p1 = wrapped >> 1;
    return wrapped;
}

int brrDecodeBlock(const uint8_t* b, BrrFilterState& st, int16_t out[kBrrSamplesPerBlock]) {
    const int range = b[0] >> 4, filter = (b[0] >> 2) & 3;
    for (int i = 0; i < kBrrSamplesPerBlock; ++i) {
        int n = (i & 1) ? (b[1 + i / 2] & 15) : (b[1 + i / 2] >> 4);  // high nibble first
        if (n >= 8) n -= 16;
        out[i] = int16_t(brrDecodeNibble(n, range, filter, st));
    }
    return b[0] & 3;
}

BrrDecodeInfo brrDecode(const uint8_t* data, size_t blocks, int16_t* out, size_t cap) {
    BrrDecodeInfo info;
    BrrFilterState st;
    for (size_t b = 0; b < blocks && info.samples + kBrrSamplesPerBlock <= cap; ++b) {
        const int flags = brrDecodeBlock(data + b * kBrrBytesPerBlock, st, out + info.samples);
        info.samples += kBrrSamplesPerBlock;
        ++info.blocks;
        if (flags & 1) { info.endFlag = true; info.loopFlag = (flags & 2) != 0; break; }
    }
    return info;
}

size_t brrEncodedSize(size_t n) { return std::max<size_t>(1, (n + 15) / 16) * kBrrBytesPerBlock; }

size_t brrEncode(const int16_t* pcm, size_t count, const BrrEncodeOptions& opt, uint8_t* out, size_t cap) {
    const size_t blocks = std::max<size_t>(1, (count + 15) / 16);
    if (cap < blocks * kBrrBytesPerBlock) return 0;
    const size_t loopBlock = opt.loopStartSample / 16;
    BrrFilterState st;
    for (size_t b = 0; b < blocks; ++b) {
        int x[16];
        for (size_t i = 0; i < 16; ++i) x[i] = b * 16 + i < count ? pcm[b * 16 + i] : 0;
        const int filters = (b == 0 || (opt.loop && b == loopBlock)) ? 1 : 4;  // loop entry must not depend on history
        double bestErr = 1e300;
        int bestR = 0, bestF = 0, bestN[16] = {};
        BrrFilterState bestSt;
        for (int f = 0; f < filters; ++f)
            for (int r = 0; r <= 12; ++r) {
                BrrFilterState s = st;
                int ns[16];
                double err = 0;
                for (int i = 0; i < 16; ++i) {
                    BrrFilterState probe = s;
                    const int base = brrDecodeNibble(0, r, f, probe);  // prediction with a zero residual
                    const int n = std::clamp((x[i] - base + (1 << r) / 2) >> r, -8, 7);  // output step is 2^r
                    const int y = brrDecodeNibble(n, r, f, s);
                    ns[i] = n;
                    err += double(x[i] - y) * double(x[i] - y);
                }
                if (err < bestErr) { bestErr = err; bestR = r; bestF = f; bestSt = s; std::copy(ns, ns + 16, bestN); }
            }
        uint8_t* o = out + b * kBrrBytesPerBlock;
        o[0] = uint8_t((bestR << 4) | (bestF << 2) | (b + 1 == blocks ? (1 | (opt.loop ? 2 : 0)) : 0));
        for (int i = 0; i < 16; i += 2) o[1 + i / 2] = uint8_t(((bestN[i] & 15) << 4) | (bestN[i + 1] & 15));
        st = bestSt;
    }
    return blocks * kBrrBytesPerBlock;
}

}  // namespace aur::audio
