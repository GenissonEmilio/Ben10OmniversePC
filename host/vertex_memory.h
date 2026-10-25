#pragma once
// Quadword LS operations for the verified, diagnostic-free vertex loop.
// Call only after ben10_vertex_cache_safe() accepts the current context.
#include "spu_context.h"
#include <tmmintrin.h>
#include <stddef.h>

static inline int ben10_vertex_ranges_overlap(uintptr_t a, size_t an, uintptr_t b, size_t bn) {
    return a <= b ? b - a < an : a - b < bn;
}
static inline int ben10_vertex_cache_safe(const spu_context* ctx) {
    if ((ctx->image_id != 100 && ctx->image_id != 102) || !ctx->ls || g_spu_ls_probe || g_spu_smc_watch || g_spu_ls_watch_n)
        return 0;
    const uintptr_t ls = (uintptr_t)ctx->ls;
    // The normal ls_store lives between the register bank and metadata.
    // A raw/external LS is also valid, provided it cannot alias either region.
    if (ben10_vertex_ranges_overlap(ls, SPU_LS_SIZE, (uintptr_t)ctx->gpr, sizeof ctx->gpr)) return 0;
    return !ben10_vertex_ranges_overlap(ls, SPU_LS_SIZE, (uintptr_t)&ctx->ls,
        sizeof(*ctx) - offsetof(spu_context, ls));
}
static inline __attribute__((always_inline)) u128 ben10_vertex_ls_read128(const spu_context* ctx, uint32_t address) {
    const __m128i order = _mm_setr_epi8(3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12);
    const __m128i value = _mm_shuffle_epi8(
        _mm_loadu_si128((const __m128i*)(ctx->ls + (address & 0x3fff0u))), order);
    u128 out; memcpy(&out, &value, 16); return out;
}
static inline __attribute__((always_inline)) void ben10_vertex_ls_write128(spu_context* ctx, uint32_t address, u128 value) {
    const __m128i order = _mm_setr_epi8(3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12);
    __m128i raw; memcpy(&raw, &value, 16);
    _mm_storeu_si128((__m128i*)(ctx->ls + (address & 0x3fff0u)), _mm_shuffle_epi8(raw, order));
}
