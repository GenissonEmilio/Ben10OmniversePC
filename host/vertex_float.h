#pragma once
// Exact x86 single precision for the identified vertex kernel only.
// Compile with -mfma -frounding-math: constant folding must honor dynamic MXCSR.
// IEEE RTZ overflow saturates to 0x7f7fffff, below the SPU extended range;
// those lanes and exponent-255 operands must use the exact integer oracle.
// Keep exceptional bodies out of the hot register allocation. Clang can also
// preserve the caller registers on this rare path, avoiding spills in the loop.
// ALWAYS_SCOPED is only set for image 100, entered through the runtime hook;
// the boot adapter rejects CPUs without the instructions required by this TU.
#include "spu_helpers.h"
#include "vertex_float_env.h"
#include <immintrin.h>
#if defined(__clang__) && __has_attribute(preserve_all)
#define BEN10_VERTEX_COLD __attribute__((noinline,cold,preserve_all))
#else
#define BEN10_VERTEX_COLD __attribute__((noinline,cold))
#endif
static BEN10_VERTEX_COLD u128 ben10_vertex_fix(u128 out, int good, u128 a, u128 b, u128 c, int op) {
    for (int i = 0; i < 4; ++i) {
        if (good & (1 << i)) continue;
        const unsigned x = a._u32[i], y = b._u32[i], z = c._u32[i];
        switch (op) {
        case 0: out._u32[i] = spu__fa_lane(x, y); break;
        case 1: out._u32[i] = spu__fa_lane(x, y ^ 0x80000000u); break;
        case 2: out._u32[i] = spu__fm_lane(x, y); break;
        case 3: out._u32[i] = spu__fma_lane(x, y, z, 0, 0); break;
        case 4: out._u32[i] = spu__fma_lane(x, y, z, 0, 0x80000000u); break;
        default: out._u32[i] = spu__fma_lane(x, y, z, 0x80000000u, 0); break;
        }
    }
    return out;
}
static inline __attribute__((always_inline)) u128 ben10_vertex_op(u128 a, u128 b, u128 c, int op) {
    const __m128i x = spu__x8_ld(a), y = spu__x8_ld(b), z = spu__x8_ld(c);
    const __m128 A = _mm_castsi128_ps(x), B = _mm_castsi128_ps(y), C = _mm_castsi128_ps(z);
    __m128 result;
    switch (op) {
    case 0: result = _mm_add_ps(A, B); break;
    case 1: result = _mm_sub_ps(A, B); break;
    case 2: result = _mm_mul_ps(A, B); break;
    case 3: result = _mm_fmadd_ps(A, B, C); break;
    case 4: result = _mm_fmsub_ps(A, B, C); break;
    default: result = _mm_fnmadd_ps(A, B, C); break;
    }
    const __m128i exponent = _mm_set1_epi32(0x7f800000), raw = _mm_castps_si128(result);
    __m128i bad = _mm_or_si128(_mm_cmpeq_epi32(_mm_and_si128(x, exponent), exponent),
                              _mm_cmpeq_epi32(_mm_and_si128(y, exponent), exponent));
    if (op >= 3) bad = _mm_or_si128(bad, _mm_cmpeq_epi32(_mm_and_si128(z, exponent), exponent));
    const __m128i magnitude = _mm_and_si128(raw, _mm_set1_epi32(0x7fffffff));
    // RTZ overflows saturate to IEEE max finite. Recompute that boundary too.
    const __m128i valid = _mm_andnot_si128(bad, _mm_cmpgt_epi32(_mm_set1_epi32(0x7f7fffff), magnitude));
    // SPU denormals and both signed zeros are represented as positive zero.
    const __m128i cleared = _mm_andnot_si128(
        _mm_cmpeq_epi32(_mm_and_si128(raw, exponent), _mm_setzero_si128()), raw);
    const int good = _mm_movemask_ps(_mm_castsi128_ps(valid));
    const u128 out = spu__x8_st(cleared);
    return good == 15 ? out : ben10_vertex_fix(out, good, a, b, c, op);
}
static BEN10_VERTEX_COLD u128 ben10_vertex_original_fa(u128 a,u128 b) {return spu_fa(a,b);}
static BEN10_VERTEX_COLD u128 ben10_vertex_original_fs(u128 a,u128 b) {return spu_fs(a,b);}
static BEN10_VERTEX_COLD u128 ben10_vertex_original_fm(u128 a,u128 b) {return spu_fm(a,b);}
static BEN10_VERTEX_COLD u128 ben10_vertex_original_fma(u128 a,u128 b,u128 c) {return spu_fma(a,b,c);}
static BEN10_VERTEX_COLD u128 ben10_vertex_original_fms(u128 a,u128 b,u128 c) {return spu_fms(a,b,c);}
static BEN10_VERTEX_COLD u128 ben10_vertex_original_fnms(u128 a,u128 b,u128 c) {return spu_fnms(a,b,c);}
static inline __attribute__((always_inline)) u128 ben10_vertex_fa(u128 a,u128 b) {
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    if (!ben10_vertex_rtz_active) return ben10_vertex_original_fa(a,b);
#endif
    return ben10_vertex_op(a,b,spu_zero(),0);
}
static inline __attribute__((always_inline)) u128 ben10_vertex_fs(u128 a,u128 b) {
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    if (!ben10_vertex_rtz_active) return ben10_vertex_original_fs(a,b);
#endif
    return ben10_vertex_op(a,b,spu_zero(),1);
}
static inline __attribute__((always_inline)) u128 ben10_vertex_fm(u128 a,u128 b) {
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    if (!ben10_vertex_rtz_active) return ben10_vertex_original_fm(a,b);
#endif
    return ben10_vertex_op(a,b,spu_zero(),2);
}
static inline __attribute__((always_inline)) u128 ben10_vertex_fma(u128 a,u128 b,u128 c) {
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    if (!ben10_vertex_rtz_active) return ben10_vertex_original_fma(a,b,c);
#endif
    return ben10_vertex_op(a,b,c,3);
}
static inline __attribute__((always_inline)) u128 ben10_vertex_fms(u128 a,u128 b,u128 c) {
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    if (!ben10_vertex_rtz_active) return ben10_vertex_original_fms(a,b,c);
#endif
    return ben10_vertex_op(a,b,c,4);
}
static inline __attribute__((always_inline)) u128 ben10_vertex_fnms(u128 a,u128 b,u128 c) {
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    if (!ben10_vertex_rtz_active) return ben10_vertex_original_fnms(a,b,c);
#endif
    return ben10_vertex_op(a,b,c,5);
}
static inline u128 ben10_vertex_dfa(u128 a,u128 b) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfa(a,b);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_dfs(u128 a,u128 b) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfs(a,b);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_dfm(u128 a,u128 b) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfm(a,b);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_dfma(u128 a,u128 b,u128 c) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfma(a,b,c);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_dfms(u128 a,u128 b,u128 c) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfms(a,b,c);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_dfnms(u128 a,u128 b,u128 c) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfnms(a,b,c);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_dfnma(u128 a,u128 b,u128 c) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_dfnma(a,b,c);_mm_setcsr(saved);return out;
}
static inline u128 ben10_vertex_frds(u128 a) {
 unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0xe040u);
 u128 out=spu_frds(a);_mm_setcsr(saved);return out;
}
#define spu_fa(a,b) ben10_vertex_fa(a,b)
#define spu_fs(a,b) ben10_vertex_fs(a,b)
#define spu_fm(a,b) ben10_vertex_fm(a,b)
#define spu_fma(a,b,c) ben10_vertex_fma(a,b,c)
#define spu_fms(a,b,c) ben10_vertex_fms(a,b,c)
#define spu_fnms(a,b,c) ben10_vertex_fnms(a,b,c)
#define spu_dfa(a,b) ben10_vertex_dfa(a,b)
#define spu_dfs(a,b) ben10_vertex_dfs(a,b)
#define spu_dfm(a,b) ben10_vertex_dfm(a,b)
#define spu_dfma(a,b,c) ben10_vertex_dfma(a,b,c)
#define spu_dfms(a,b,c) ben10_vertex_dfms(a,b,c)
#define spu_dfnms(a,b,c) ben10_vertex_dfnms(a,b,c)
#define spu_dfnma(a,b,c) ben10_vertex_dfnma(a,b,c)
#define spu_frds(a) ben10_vertex_frds(a)
