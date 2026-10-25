/* Independent word-level oracles: avoid the SDK's signed-left-shift MPYH. */
#include "vertex_integer.h"
#undef spu_fi
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
static void check_half_control(u128 a, u128 b, int32_t immediate) {
    const u128 actual[2] = {spu_chd(a,immediate), spu_chx(a,b)};
    const uint32_t sums[2] = {a._u32[0] + (uint32_t)immediate, a._u32[0] + b._u32[0]};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned position = sums[k] & 14u;
        for (unsigned byte = 0; byte < 16; ++byte) {
            const unsigned expected = byte == position ? 2 : byte == position + 1 ? 3 : 16 + byte;
            assert(actual[k]._u8[byte ^ 3] == expected);
        }
        const u128 shuffled = spu_shufb(a,b,actual[k]);
        for (unsigned byte = 0; byte < 16; ++byte) {
            const unsigned expected = byte == position ? a._u8[1] :
                byte == position + 1 ? a._u8[0] : b._u8[byte ^ 3];
            assert(shuffled._u8[byte ^ 3] == expected);
        }
    }
}
static void check(u128 a, u128 b, int32_t immediate) {
    check_half_control(a,b,immediate);
    const u128 interpolated = ben10_vertex_fi(a,b), reference = spu_fi(a,b);
    assert(!memcmp(&interpolated,&reference,16));
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
    for (unsigned pos = 0; pos < 32; ++pos) for (int immediate = -64; immediate <= 63; ++immediate) {
        check_half_control(spu_splat_u32(pos),spu_splat_u32(0xffffffffu-pos),immediate);
    }
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
    puts("PASS vertex integers: 2000000 vectors, six operations, FI, CHD/CHX and SHUFB composition");
}
