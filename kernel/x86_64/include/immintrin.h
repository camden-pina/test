#ifndef __IMMINTRIN_H
#define __IMMINTRIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xmmintrin.h"
#include "emmintrin.h"

/* Define vector types for AVX (256-bit vectors) */

/* For single-precision floating point */
typedef union __attribute__((__aligned__(32), __may_alias__)) {
    float m256_f32[8];
} __m256;

/* For double-precision floating point */
typedef union __attribute__((__aligned__(32), __may_alias__)) {
    double m256_f64[4];
} __m256d;

/* For integer vectors */
typedef union __attribute__((__aligned__(32), __may_alias__)) {
    /* Vector types using GCC’s vector extensions */
    char          __v32qi  __attribute__((__vector_size__(32)));
    short         __v16hi  __attribute__((__vector_size__(32)));
    int           __v8si   __attribute__((__vector_size__(32)));
    long long     __v4di   __attribute__((__vector_size__(32)));
    /* Array representations for direct access */
    char          m256i_i8[32];
    short         m256i_i16[16];
    int           m256i_i32[8];
    long long     m256i_i64[4];
} __m256i;

/* -------------------- */
/* AVX Intrinsics       */
/* -------------------- */

/* Single-precision floating-point intrinsics */

/* Set all eight single-precision floats to zero */
static inline __m256 _mm256_setzero_ps(void) {
    __m256 res;
    res.m256_f32[0] = 0.0f; res.m256_f32[1] = 0.0f;
    res.m256_f32[2] = 0.0f; res.m256_f32[3] = 0.0f;
    res.m256_f32[4] = 0.0f; res.m256_f32[5] = 0.0f;
    res.m256_f32[6] = 0.0f; res.m256_f32[7] = 0.0f;
    return res;
}

/* Set all eight single-precision floats to a specified value */
static inline __m256 _mm256_set1_ps(float a) {
    __m256 res;
    res.m256_f32[0] = a; res.m256_f32[1] = a;
    res.m256_f32[2] = a; res.m256_f32[3] = a;
    res.m256_f32[4] = a; res.m256_f32[5] = a;
    res.m256_f32[6] = a; res.m256_f32[7] = a;
    return res;
}

/* Load 256 bits (8 floats) from an aligned memory address */
static inline __m256 _mm256_load_ps(const float *p) {
    __m256 res;
    for (int i = 0; i < 8; i++) {
        res.m256_f32[i] = p[i];
    }
    return res;
}

/* Store 256 bits (8 floats) to an aligned memory address */
static inline void _mm256_store_ps(float *p, __m256 a) {
    for (int i = 0; i < 8; i++) {
        p[i] = a.m256_f32[i];
    }
}

/* Add packed single-precision floating-point elements */
static inline __m256 _mm256_add_ps(__m256 a, __m256 b) {
    __m256 res;
    for (int i = 0; i < 8; i++) {
        res.m256_f32[i] = a.m256_f32[i] + b.m256_f32[i];
    }
    return res;
}

/* Subtract packed single-precision floating-point elements */
static inline __m256 _mm256_sub_ps(__m256 a, __m256 b) {
    __m256 res;
    for (int i = 0; i < 8; i++) {
        res.m256_f32[i] = a.m256_f32[i] - b.m256_f32[i];
    }
    return res;
}

/* Double-precision floating-point intrinsics */

/* Set all four double-precision floats to zero */
static inline __m256d _mm256_setzero_pd(void) {
    __m256d res;
    res.m256_f64[0] = 0.0; res.m256_f64[1] = 0.0;
    res.m256_f64[2] = 0.0; res.m256_f64[3] = 0.0;
    return res;
}

/* Set all four double-precision floats to a specified value */
static inline __m256d _mm256_set1_pd(double a) {
    __m256d res;
    res.m256_f64[0] = a; res.m256_f64[1] = a;
    res.m256_f64[2] = a; res.m256_f64[3] = a;
    return res;
}

/* Load 256 bits (4 doubles) from an aligned memory address */
static inline __m256d _mm256_load_pd(const double *p) {
    __m256d res;
    for (int i = 0; i < 4; i++) {
        res.m256_f64[i] = p[i];
    }
    return res;
}

/* Store 256 bits (4 doubles) to an aligned memory address */
static inline void _mm256_store_pd(double *p, __m256d a) {
    for (int i = 0; i < 4; i++) {
        p[i] = a.m256_f64[i];
    }
}

/* Integer vector intrinsics */

/* Load a 256-bit integer vector from an aligned memory address */
static inline __m256i _mm256_load_si256(const __m256i *p) {
    return *p;
}

/* Store a 256-bit integer vector to an aligned memory address */
static inline void _mm256_store_si256(__m256i *p, __m256i a) {
    *p = a;
}

/* Load a 256-bit integer vector from an unaligned memory address */
static inline __m256i _mm256_loadu_si256(const __m256i *p) {
    __m256i res;
    __builtin_memcpy(&res, p, sizeof(res));
    return res;
}

/* Store a 256-bit integer vector to an unaligned memory address */
static inline void _mm256_storeu_si256(__m256i *p, __m256i a) {
    __builtin_memcpy(p, &a, sizeof(a));
}

#ifdef __cplusplus
}
#endif

#endif /* __IMMINTRIN_H */
