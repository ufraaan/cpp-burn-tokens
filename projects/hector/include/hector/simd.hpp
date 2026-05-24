#pragma once

#include <cmath>
#include <cstddef>

#if defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace hector {

namespace simd {

inline float l2_squared(const float* a, const float* b, int n) noexcept {
#if defined(__ARM_NEON__)
    float32x4_t sum = vdupq_n_f32(0);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        float32x4_t d = vsubq_f32(va, vb);
        sum = vmlaq_f32(sum, d, d);
    }
    float r = vaddvq_f32(sum);
    for (; i < n; ++i) {
        float d = a[i] - b[i];
        r += d * d;
    }
    return r;
#else
    float r = 0;
    for (int i = 0; i < n; ++i) {
        float d = a[i] - b[i];
        r += d * d;
    }
    return r;
#endif
}

inline float dot(const float* a, const float* b, int n) noexcept {
#if defined(__ARM_NEON__)
    float32x4_t sum = vdupq_n_f32(0);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        sum = vmlaq_f32(sum, va, vb);
    }
    float r = vaddvq_f32(sum);
    for (; i < n; ++i) {
        r += a[i] * b[i];
    }
    return r;
#else
    float r = 0;
    for (int i = 0; i < n; ++i) {
        r += a[i] * b[i];
    }
    return r;
#endif
}

inline float l2(const float* a, const float* b, int n) noexcept {
    return std::sqrt(l2_squared(a, b, n));
}

} // namespace simd

} // namespace hector
