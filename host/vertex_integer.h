#pragma once
// Packed x86 forms of the halfword operations used by the vertex job.
// Arithmetic is modulo 2^32; no signed C shift or overflow is required.
#include "spu_helpers.h"
#include <immintrin.h>

static inline __attribute__((always_inline)) u128 ben10_vertex_mpyu(u128 a, u128 b) {
    const __m128i mask = _mm_set1_epi32(0xffff);
    return spu__x8_st(_mm_mullo_epi32(_mm_and_si128(spu__x8_ld(a), mask),
                                    _mm_and_si128(spu__x8_ld(b), mask)));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_mpyh(u128 a, u128 b) {
    return spu__x8_st(_mm_mullo_epi32(_mm_and_si128(spu__x8_ld(a), _mm_set1_epi32(-65536)),
                                    spu__x8_ld(b)));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_mpyhhu(u128 a, u128 b) {
    return spu__x8_st(_mm_mullo_epi32(_mm_srli_epi32(spu__x8_ld(a), 16),
                                    _mm_srli_epi32(spu__x8_ld(b), 16)));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_xshw(u128 a) {
    return spu__x8_st(_mm_srai_epi32(_mm_slli_epi32(spu__x8_ld(a), 16), 16));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_mpyi(u128 a, int32_t immediate) {
    // Explicit sign extension avoids implementation-defined int16 conversion.
    const uint32_t low = (uint32_t)immediate & 0xffffu;
    const int32_t signed_low = (int32_t)(low ^ 0x8000u) - 0x8000;
    const __m128i value = _mm_srai_epi32(_mm_slli_epi32(spu__x8_ld(a), 16), 16);
    return spu__x8_st(_mm_mullo_epi32(value, _mm_set1_epi32(signed_low)));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_mpyui(u128 a, int32_t immediate) {
    const __m128i value = _mm_and_si128(spu__x8_ld(a), _mm_set1_epi32(0xffff));
    return spu__x8_st(_mm_mullo_epi32(value, _mm_set1_epi32((uint32_t)immediate & 0xffffu)));
}

static inline __attribute__((always_inline)) u128 ben10_vertex_half_control(uint32_t position) {
    // Eight possible halfword insertions, in host byte order. Loading the
    // complete value avoids overlapping byte stores followed by a vector load.
    static const uint16_t controls[8][8] = {
        {0x1213,0x0203,0x1617,0x1415,0x1a1b,0x1819,0x1e1f,0x1c1d},
        {0x0203,0x1011,0x1617,0x1415,0x1a1b,0x1819,0x1e1f,0x1c1d},
        {0x1213,0x1011,0x1617,0x0203,0x1a1b,0x1819,0x1e1f,0x1c1d},
        {0x1213,0x1011,0x0203,0x1415,0x1a1b,0x1819,0x1e1f,0x1c1d},
        {0x1213,0x1011,0x1617,0x1415,0x1a1b,0x0203,0x1e1f,0x1c1d},
        {0x1213,0x1011,0x1617,0x1415,0x0203,0x1819,0x1e1f,0x1c1d},
        {0x1213,0x1011,0x1617,0x1415,0x1a1b,0x1819,0x1e1f,0x0203},
        {0x1213,0x1011,0x1617,0x1415,0x1a1b,0x1819,0x0203,0x1c1d},
    };
    u128 out; memcpy(&out, controls[(position >> 1) & 7], 16); return out;
}
static inline __attribute__((always_inline)) u128 ben10_vertex_chd(u128 a, int32_t immediate) {
    return ben10_vertex_half_control(a._u32[0] + (uint32_t)immediate);
}
static inline __attribute__((always_inline)) u128 ben10_vertex_chx(u128 a, u128 b) {
    return ben10_vertex_half_control(a._u32[0] + b._u32[0]);
}
static inline __attribute__((always_inline)) __m128i ben10_vertex_low_words(__m256i value) {
    return _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(value,
        _mm256_setr_epi32(0,2,4,6,0,2,4,6)));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_fi(u128 a, u128 b) {
    // Four exact fixed-point interpolations. The 10-bit step times the
    // 19-bit fraction fits uint32; the subtraction needs 33-bit magnitudes.
    const __m128i A = spu__x8_ld(a), B = spu__x8_ld(b);
    const __m128i base = _mm_or_si128(_mm_and_si128(_mm_srli_epi32(B,10),
        _mm_set1_epi32(0x1fff)), _mm_set1_epi32(0x2000));
    const __m128i product = _mm_mullo_epi32(_mm_and_si128(B,_mm_set1_epi32(0x3ff)),
        _mm_and_si128(A,_mm_set1_epi32(0x7ffff)));
    __m256i magnitude = _mm256_sub_epi64(_mm256_slli_epi64(_mm256_cvtepu32_epi64(base),19),
        _mm256_cvtepu32_epi64(product));
    const __m256i normalize = _mm256_cmpgt_epi64(_mm256_set1_epi64x(0x100000000LL),magnitude);
    magnitude = _mm256_blendv_epi8(magnitude,_mm256_slli_epi64(magnitude,1),normalize);
    const __m128i exponent = _mm_add_epi32(_mm_and_si128(_mm_srli_epi32(B,23),
        _mm_set1_epi32(255)),ben10_vertex_low_words(normalize));
    const __m128i fraction = _mm_and_si128(ben10_vertex_low_words(_mm256_srli_epi64(magnitude,9)),
        _mm_set1_epi32(0x7fffff));
    const __m128i result = _mm_or_si128(_mm_and_si128(B,_mm_set1_epi32((int)0x80000000u)),
        _mm_or_si128(_mm_slli_epi32(exponent,23),fraction));
    return spu__x8_st(_mm_and_si128(result,_mm_cmpgt_epi32(exponent,_mm_setzero_si128())));
}

#define spu_mpyu(a,b) ben10_vertex_mpyu(a,b)
#define spu_mpyh(a,b) ben10_vertex_mpyh(a,b)
#define spu_mpyhhu(a,b) ben10_vertex_mpyhhu(a,b)
#define spu_xshw(a) ben10_vertex_xshw(a)
#define spu_mpyi(a,i) ben10_vertex_mpyi(a,i)
#define spu_mpyui(a,i) ben10_vertex_mpyui(a,i)
#define spu_chd(a,i) ben10_vertex_chd(a,i)
#define spu_chx(a,b) ben10_vertex_chx(a,b)
#define spu_fi(a,b) ben10_vertex_fi(a,b)
