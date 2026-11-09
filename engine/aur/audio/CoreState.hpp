// CoreState.hpp - tiny visitor sinks for instance state (no JSON, no allocation).
// A core exposes `template <class V> void visit(V&)` and calls v(field) / v.bytes(p, n).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace aur::audio {

class StateSizer {
public:
    template <class T> void operator()(T&) { static_assert(std::is_trivially_copyable_v<T>); n_ += sizeof(T); }
    void bytes(void*, size_t n) { n_ += n; }
    size_t size() const { return n_; }
private:
    size_t n_ = 0;
};

class StateWriter {
public:
    StateWriter(uint8_t* dst, size_t cap) : d_(dst), cap_(cap) {}
    template <class T> void operator()(T& v) { static_assert(std::is_trivially_copyable_v<T>); bytes(&v, sizeof(T)); }
    void bytes(void* p, size_t n) {
        if (!ok_ || n_ + n > cap_) { ok_ = false; return; }
        std::memcpy(d_ + n_, p, n);
        n_ += n;
    }
    bool ok() const { return ok_; }
    size_t size() const { return n_; }
private:
    uint8_t* d_; size_t cap_, n_ = 0; bool ok_ = true;
};

class StateReader {
public:
    StateReader(const uint8_t* src, size_t len) : s_(src), len_(len) {}
    template <class T> void operator()(T& v) { static_assert(std::is_trivially_copyable_v<T>); bytes(&v, sizeof(T)); }
    void bytes(void* p, size_t n) {
        if (!ok_ || n_ + n > len_) { ok_ = false; return; }
        std::memcpy(p, s_ + n_, n);
        n_ += n;
    }
    bool ok() const { return ok_; }
    size_t size() const { return n_; }
private:
    const uint8_t* s_; size_t len_, n_ = 0; bool ok_ = true;
};

}  // namespace aur::audio
