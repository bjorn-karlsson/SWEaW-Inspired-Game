#pragma once

#include <cstdint>

namespace gc {

/// Small, fast, fully deterministic PRNG (xorshift64*). Determinism matters:
/// the same seed must always produce the same campaign so autoresolve results
/// are reproducible and tests are stable.
class Rng {
public:
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : state_(seed ? seed : 1ull) {}

    void seed(uint64_t s) { state_ = s ? s : 1ull; }
    uint64_t rawState() const { return state_; }

    uint64_t next() {
        uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        return x * 0x2545F4914F6CDD1Dull;
    }

    /// Uniform float in [0, 1).
    float unit() { return static_cast<float>((next() >> 40)) / 16777216.0f; }

    /// Uniform float in [lo, hi).
    float range(float lo, float hi) { return lo + unit() * (hi - lo); }

    /// Uniform integer in [lo, hi] inclusive.
    int intRange(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<uint64_t>(hi - lo + 1));
    }

    bool chance(float p) { return unit() < p; }

private:
    uint64_t state_;
};

}  // namespace gc
