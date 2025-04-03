#ifndef __EMMINTRIN_H
#define __EMMINTRIN_H

#include "xmmintrin.h"
#include <string.h>  /* for __builtin_memcpy */

/* Define vector types for our intrinsics */
typedef char          __v16qi  __attribute__((__vector_size__(16)));
typedef short         __v8hi   __attribute__((__vector_size__(16)));
typedef int           __v4si   __attribute__((__vector_size__(16)));
typedef long long     __v2di   __attribute__((__vector_size__(16)));

/* Define __m128i as a union of several vector types.
   This lets us access the same 128 bits as 16 8‑bit, 8 16‑bit, 4 32‑bit,
   or 2 64‑bit values. */
typedef union __attribute__((__aligned__(16), __may_alias__)) {
    __v16qi m128i_i8;
    __v8hi  m128i_i16;
    __v4si  m128i_i32;
    __v2di  m128i_i64;
} __m128i;

#ifdef __cplusplus
extern "C" {
#endif

/* Set all bits to zero */
static inline __m128i _mm_setzero_si128(void) {
    __m128i res;
    res.m128i_i8 = (__v16qi){0};
    return res;
}

/* Set every 8-bit element to 'a' */
static inline __m128i _mm_set1_epi8(char a) {
    __m128i res;
    res.m128i_i8 = (__v16qi){a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a};
    return res;
}

/* Set every 16-bit element to 'a' */
static inline __m128i _mm_set1_epi16(short a) {
    __m128i res;
    res.m128i_i16 = (__v8hi){a, a, a, a, a, a, a, a};
    return res;
}

/* Set every 32-bit element to 'a' */
static inline __m128i _mm_set1_epi32(int a) {
    __m128i res;
    res.m128i_i32 = (__v4si){a, a, a, a};
    return res;
}

/* Set every 64-bit element to 'a' */
static inline __m128i _mm_set1_epi64x(long long a) {
    __m128i res;
    res.m128i_i64 = (__v2di){a, a};
    return res;
}

/* Initialize with 16 8-bit integers.
   Note the reverse order: the first parameter becomes the most significant byte. */
static inline __m128i _mm_set_epi8(char e15, char e14, char e13, char e12,
                                   char e11, char e10, char e9,  char e8,
                                   char e7,  char e6, char e5,  char e4,
                                   char e3,  char e2, char e1,  char e0) {
    __m128i res;
    res.m128i_i8 = (__v16qi){ e0, e1, e2, e3, e4, e5, e6, e7,
                               e8, e9, e10, e11, e12, e13, e14, e15 };
    return res;
}

/* Initialize with 8 16-bit integers */
static inline __m128i _mm_set_epi16(short e7, short e6, short e5, short e4,
                                    short e3, short e2, short e1, short e0) {
    __m128i res;
    res.m128i_i16 = (__v8hi){ e0, e1, e2, e3, e4, e5, e6, e7 };
    return res;
}

/* Initialize with 4 32-bit integers */
static inline __m128i _mm_set_epi32(int e3, int e2, int e1, int e0) {
    __m128i res;
    res.m128i_i32 = (__v4si){ e0, e1, e2, e3 };
    return res;
}

/* Initialize with 2 64-bit integers */
static inline __m128i _mm_set_epi64x(long long e1, long long e0) {
    __m128i res;
    res.m128i_i64 = (__v2di){ e0, e1 };
    return res;
}

/* Load a 128-bit integer from an aligned memory location */
static inline __m128i _mm_load_si128(const __m128i *p) {
    return *p;
}

/* Store a 128-bit integer to an aligned memory location */
static inline void _mm_store_si128(__m128i *p, __m128i a) {
    *p = a;
}

/* Load a 128-bit integer from an unaligned memory location */
static inline __m128i _mm_loadu_si128(const __m128i *p) {
    __m128i res;
    __builtin_memcpy(&res, p, sizeof(res));
    return res;
}

/* Store a 128-bit integer to an unaligned memory location */
static inline void _mm_storeu_si128(__m128i *p, __m128i a) {
    __builtin_memcpy(p, &a, sizeof(a));
}

/* Add packed 8-bit integers */
static inline __m128i _mm_add_epi8(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i8 = a.m128i_i8 + b.m128i_i8;
    return res;
}

/* Add packed 16-bit integers */
static inline __m128i _mm_add_epi16(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i16 = a.m128i_i16 + b.m128i_i16;
    return res;
}

/* Add packed 32-bit integers */
static inline __m128i _mm_add_epi32(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i32 = a.m128i_i32 + b.m128i_i32;
    return res;
}

/* Add packed 64-bit integers */
static inline __m128i _mm_add_epi64(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i64 = a.m128i_i64 + b.m128i_i64;
    return res;
}

/* Subtract packed 8-bit integers */
static inline __m128i _mm_sub_epi8(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i8 = a.m128i_i8 - b.m128i_i8;
    return res;
}

/* Subtract packed 16-bit integers */
static inline __m128i _mm_sub_epi16(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i16 = a.m128i_i16 - b.m128i_i16;
    return res;
}

/* Subtract packed 32-bit integers */
static inline __m128i _mm_sub_epi32(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i32 = a.m128i_i32 - b.m128i_i32;
    return res;
}

/* Subtract packed 64-bit integers */
static inline __m128i _mm_sub_epi64(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i64 = a.m128i_i64 - b.m128i_i64;
    return res;
}

/* Bitwise AND of 128-bit integers */
static inline __m128i _mm_and_si128(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i8 = a.m128i_i8 & b.m128i_i8;
    return res;
}

/* Bitwise OR of 128-bit integers */
static inline __m128i _mm_or_si128(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i8 = a.m128i_i8 | b.m128i_i8;
    return res;
}

/* Bitwise XOR of 128-bit integers */
static inline __m128i _mm_xor_si128(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i8 = a.m128i_i8 ^ b.m128i_i8;
    return res;
}

/* Bitwise AND NOT: ~a & b */
static inline __m128i _mm_andnot_si128(__m128i a, __m128i b) {
    __m128i res;
    res.m128i_i8 = (~a.m128i_i8) & b.m128i_i8;
    return res;
}

/* Shift left each 16-bit element by an immediate value (mask imm8 to 4 bits) */
static inline __m128i _mm_slli_epi16(__m128i a, int imm8) {
    __m128i res;
    res.m128i_i16 = a.m128i_i16 << (imm8 & 0x0F);
    return res;
}

/* Shift left each 32-bit element by an immediate value (mask imm8 to 5 bits) */
static inline __m128i _mm_slli_epi32(__m128i a, int imm8) {
    __m128i res;
    res.m128i_i32 = a.m128i_i32 << (imm8 & 0x1F);
    return res;
}

/* Shift left each 64-bit element by an immediate value (mask imm8 to 6 bits) */
static inline __m128i _mm_slli_epi64(__m128i a, int imm8) {
    __m128i res;
    res.m128i_i64 = a.m128i_i64 << (imm8 & 0x3F);
    return res;
}

/* Logical right shift of each 16-bit element (mask imm8 to 4 bits) */
static inline __m128i _mm_srli_epi16(__m128i a, int imm8) {
    __m128i res;
    res.m128i_i16 = (__v8hi)((__v8hi)a.m128i_i16 >> (imm8 & 0x0F));
    return res;
}

/* Logical right shift of each 32-bit element (mask imm8 to 5 bits) */
static inline __m128i _mm_srli_epi32(__m128i a, int imm8) {
    __m128i res;
    res.m128i_i32 = (__v4si)((__v4si)a.m128i_i32 >> (imm8 & 0x1F));
    return res;
}

/* Logical right shift of each 64-bit element (mask imm8 to 6 bits) */
static inline __m128i _mm_srli_epi64(__m128i a, int imm8) {
    __m128i res;
    res.m128i_i64 = (__v2di)((__v2di)a.m128i_i64 >> (imm8 & 0x3F));
    return res;
}

/* Multiply packed 16-bit integers and return the low 16 bits of each product.
   (This is implemented as an element‑wise multiplication.) */
static inline __m128i _mm_mul_epi16(__m128i a, __m128i b) {
    __m128i res;
    short *ap = (short *)&a;
    short *bp = (short *)&b;
    short *rp = (short *)&res;
    for (int i = 0; i < 8; i++) {
         rp[i] = ap[i] * bp[i];
    }
    return res;
}

/* Multiply unsigned 32-bit integers from each 64-bit element.
   It multiplies the lower 32 bits of each 64-bit lane. */
static inline __m128i _mm_mul_epu32(__m128i a, __m128i b) {
    __m128i res;
    unsigned int *ap = (unsigned int *)&a;
    unsigned int *bp = (unsigned int *)&b;
    unsigned long long *rp = (unsigned long long *)&res;
    rp[0] = (unsigned long long)ap[0] * (unsigned long long)bp[0];
    rp[1] = (unsigned long long)ap[2] * (unsigned long long)bp[2];
    return res;
}

/* Compare packed 8-bit integers for greater-than.
   Each element is set to 0xFF if a > b, or 0 otherwise. */
static inline __m128i _mm_cmpgt_epi8(__m128i a, __m128i b) {
    __m128i res;
    char *ap = (char *)&a;
    char *bp = (char *)&b;
    char *rp = (char *)&res;
    for (int i = 0; i < 16; i++) {
         rp[i] = (ap[i] > bp[i]) ? (char)0xFF : 0;
    }
    return res;
}

/* Compare packed 16-bit integers for greater-than. */
static inline __m128i _mm_cmpgt_epi16(__m128i a, __m128i b) {
    __m128i res;
    short *ap = (short *)&a;
    short *bp = (short *)&b;
    short *rp = (short *)&res;
    for (int i = 0; i < 8; i++) {
         rp[i] = (ap[i] > bp[i]) ? (short)0xFFFF : 0;
    }
    return res;
}

/* Compare packed 32-bit integers for greater-than. */
static inline __m128i _mm_cmpgt_epi32(__m128i a, __m128i b) {
    __m128i res;
    int *ap = (int *)&a;
    int *bp = (int *)&b;
    int *rp = (int *)&res;
    for (int i = 0; i < 4; i++) {
         rp[i] = (ap[i] > bp[i]) ? -1 : 0;
    }
    return res;
}

/* Unpack and interleave the low-order 8-bit integers from two vectors. */
static inline __m128i _mm_unpacklo_epi8(__m128i a, __m128i b) {
    __m128i res;
    char *ap = (char *)&a;
    char *bp = (char *)&b;
    char *rp = (char *)&res;
    for (int i = 0; i < 8; i++) {
         rp[i*2]   = ap[i];
         rp[i*2+1] = bp[i];
    }
    return res;
}

/* Unpack and interleave the high-order 8-bit integers from two vectors. */
static inline __m128i _mm_unpackhi_epi8(__m128i a, __m128i b) {
    __m128i res;
    char *ap = (char *)&a;
    char *bp = (char *)&b;
    char *rp = (char *)&res;
    for (int i = 0; i < 8; i++) {
         rp[i*2]   = ap[i+8];
         rp[i*2+1] = bp[i+8];
    }
    return res;
}

/* Unpack and interleave the low-order 16-bit integers from two vectors. */
static inline __m128i _mm_unpacklo_epi16(__m128i a, __m128i b) {
    __m128i res;
    short *ap = (short *)&a;
    short *bp = (short *)&b;
    short *rp = (short *)&res;
    for (int i = 0; i < 4; i++) {
         rp[i*2]   = ap[i];
         rp[i*2+1] = bp[i];
    }
    return res;
}

/* Unpack and interleave the high-order 16-bit integers from two vectors. */
static inline __m128i _mm_unpackhi_epi16(__m128i a, __m128i b) {
    __m128i res;
    short *ap = (short *)&a;
    short *bp = (short *)&b;
    short *rp = (short *)&res;
    for (int i = 0; i < 4; i++) {
         rp[i*2]   = ap[i+4];
         rp[i*2+1] = bp[i+4];
    }
    return res;
}

/* Unpack and interleave the low-order 32-bit integers from two vectors. */
static inline __m128i _mm_unpacklo_epi32(__m128i a, __m128i b) {
    __m128i res;
    int *ap = (int *)&a;
    int *bp = (int *)&b;
    int *rp = (int *)&res;
    rp[0] = ap[0];
    rp[1] = bp[0];
    rp[2] = ap[1];
    rp[3] = bp[1];
    return res;
}

/* Unpack and interleave the high-order 32-bit integers from two vectors. */
static inline __m128i _mm_unpackhi_epi32(__m128i a, __m128i b) {
    __m128i res;
    int *ap = (int *)&a;
    int *bp = (int *)&b;
    int *rp = (int *)&res;
    rp[0] = ap[2];
    rp[1] = bp[2];
    rp[2] = ap[3];
    rp[3] = bp[3];
    return res;
}

/* Shuffle 32-bit integers within a 128-bit vector according to an immediate value.
   The immediate value encodes the new ordering of the 4 32-bit lanes. */
static inline __m128i _mm_shuffle_epi32(__m128i a, const int imm8) {
    __m128i res;
    int *ap = (int *)&a;
    int *rp = (int *)&res;
    rp[0] = ap[(imm8 >> 0) & 0x3];
    rp[1] = ap[(imm8 >> 2) & 0x3];
    rp[2] = ap[(imm8 >> 4) & 0x3];
    rp[3] = ap[(imm8 >> 6) & 0x3];
    return res;
}

/* Extract the lower 32 bits from a 128-bit vector as an integer. */
static inline int _mm_cvtsi128_si32(__m128i a) {
    int *ap = (int *)&a;
    return ap[0];
}

/* Extract the lower 64 bits from a 128-bit vector as a long long. */
static inline long long _mm_cvtsi128_si64(__m128i a) {
    long long *ap = (long long *)&a;
    return ap[0];
}

/* 
 * Non-temporal (streaming) store of a 128-bit integer.
 * Uses the MOVNTDQ instruction to store the 128-bit value in 'a' 
 * directly to the memory location pointed to by 'p', bypassing the cache.
 */
static inline void _mm_stream_si128(__m128i *p, __m128i a) {
    __asm__ volatile ("movntdq %1, (%0)" : : "r" (p), "x" (a) : "memory");
}

/*
 * Store fence (SFENCE) to ensure ordering of non-temporal stores.
 * This instruction guarantees that all preceding non-temporal stores 
 * are globally visible before any subsequent memory operations.
 */
static inline void _mm_sfence(void) {
    __asm__ volatile ("sfence" ::: "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* __EMMINTRIN_H */
