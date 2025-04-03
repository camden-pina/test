#ifndef __XMMINTRIN_H
#define __XMMINTRIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Minimal definition of __m128 for SSE floating point operations.
   __m128 is defined as a union of 4 floats, aligned on a 16‐byte boundary. */
typedef union __attribute__((__aligned__(16))) {
    float m128_f32[4];
} __m128;

/* Set all four single‑precision floats to zero. */
static inline __m128 _mm_setzero_ps(void) {
    __m128 res;
    res.m128_f32[0] = 0.0f;
    res.m128_f32[1] = 0.0f;
    res.m128_f32[2] = 0.0f;
    res.m128_f32[3] = 0.0f;
    return res;
}

/* Set the four single‑precision floats to the specified values.
   Note that _mm_set_ps stores its parameters in reverse order:
   the first parameter becomes element 3, the last becomes element 0. */
static inline __m128 _mm_set_ps(float z, float y, float x, float w) {
    __m128 res;
    res.m128_f32[0] = w;
    res.m128_f32[1] = x;
    res.m128_f32[2] = y;
    res.m128_f32[3] = z;
    return res;
}

/* Load 128 bits (four floats) from an aligned memory address. */
static inline __m128 _mm_load_ps(const float *p) {
    __m128 res;
    res.m128_f32[0] = p[0];
    res.m128_f32[1] = p[1];
    res.m128_f32[2] = p[2];
    res.m128_f32[3] = p[3];
    return res;
}

/* Store 128 bits (four floats) to an aligned memory address. */
static inline void _mm_store_ps(float *p, __m128 a) {
    p[0] = a.m128_f32[0];
    p[1] = a.m128_f32[1];
    p[2] = a.m128_f32[2];
    p[3] = a.m128_f32[3];
}

#ifdef __cplusplus
}
#endif

#endif /* __XMMINTRIN_H */
