/* Independent word-level oracles: avoid the SDK's signed-left-shift MPYH. */
#include "vertex_integer.h"
#include <assert.h>
#include <stdio.h>
static uint32_t state = 0x10abc012;
static uint32_t random_word(void) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
static int32_t signed_half(uint32_t x) {
    return (int32_t)((x & 0xffffu) ^ 0x8000u) - 0x8000;
}
static void check(u128 a, u128 b, int32_t immediate) {
    const u128 u = spu_mpyu(a,b), h = spu_mpyh(a,b), hh = spu_mpyhhu(a,b);
    const u128 s = spu_xshw(a), si = spu_mpyi(a,immediate), ui = spu_mpyui(a,immediate);
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t x = a._u32[i], y = b._u32[i];
        assert(u._u32[i] == (x & 0xffffu) * (y & 0xffffu));
        uint32_t high_product = (uint32_t)((int64_t)signed_half(x >> 16) * signed_half(y));
        assert(h._u32[i] == (high_product << 16));
        assert(hh._u32[i] == (x >> 16) * (y >> 16));
        assert(s._u32[i] == (uint32_t)signed_half(x));
        assert(si._u32[i] == (uint32_t)((int64_t)signed_half(x) * signed_half((uint32_t)immediate)));
        assert(ui._u32[i] == (x & 0xffffu) * ((uint32_t)immediate & 0xffffu));
    }
    // Stores into the same source register must snapshot all four lanes first.
    a = spu_mpyh(a,b);
    assert(!memcmp(&a, &h, 16));
}
int main(void) {
    if (!__builtin_cpu_supports("avx2")) { puts("SKIP: AVX2 unavailable"); return 0; }
    const uint32_t edges[] = {0, 1, 0x7fff, 0x8000, 0xffff, 0x10000,
        0x7fff0000, 0x80000000, 0xffff0000, 0x7fffffff, 0x80008000, 0xffffffff};
    for (unsigned i = 0; i < 12; ++i) for (unsigned j = 0; j < 12; ++j) {
        u128 a, b;
        for (unsigned k = 0; k < 4; ++k) { a._u32[k] = edges[(i+k)%12]; b._u32[k] = edges[(j+k)%12]; }
        for (unsigned q = 0; q < 12; ++q) check(a,b,(int32_t)edges[q]);
    }
    for (unsigned n = 0; n < 2000000; ++n) {
        u128 a, b;
        for (unsigned k = 0; k < 4; ++k) { a._u32[k] = random_word(); b._u32[k] = random_word(); }
        check(a,b,(int32_t)random_word());
    }
    puts("PASS vertex integers: 2000000 vectors, six operations, signed boundaries and mixed lanes");
}
