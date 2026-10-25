#pragma once
// Packed x86 forms of the halfword operations used by the vertex job.
// Arithmetic is modulo 2^32; no signed C shift or overflow is required.
#include "spu_helpers.h"
#include <smmintrin.h>

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

#define spu_mpyu(a,b) ben10_vertex_mpyu(a,b)
#define spu_mpyh(a,b) ben10_vertex_mpyh(a,b)
#define spu_mpyhhu(a,b) ben10_vertex_mpyhhu(a,b)
#define spu_xshw(a) ben10_vertex_xshw(a)
#define spu_mpyi(a,i) ben10_vertex_mpyi(a,i)
#define spu_mpyui(a,i) ben10_vertex_mpyui(a,i)
