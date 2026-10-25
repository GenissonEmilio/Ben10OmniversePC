/* Manufactured program: aliasing, diagnostics, loop-carried regs and live-outs. */
#include "spu_helpers.h"
#include "spu_context.h"
#include <assert.h>
#include <stdio.h>
int g_spu_ls_watch_n = 0;
static unsigned watch_calls;
void spu_ls_watch_slow(uint32_t a, int b, const uint8_t* c, uint32_t d, uint32_t e) { ++watch_calls; }
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
static void job_00844E80_spu_func_000005E0(spu_context* ctx) {}
void spu_indirect_branch(spu_context* ctx) { abort(); }

void job_00844E80_spu_func_000002E0(spu_context* ctx) {
    ctx->gpr[18] = spu_il(0);
loc_fixture:
    ctx->gpr[2] = spu_ls_read128(ctx, ctx->gpr[21]._u32[0]);
    ctx->gpr[3] = spu_xor(ctx->gpr[2], ctx->gpr[4]);
    spu_ls_write128(ctx, ctx->gpr[22]._u32[0], ctx->gpr[3]);
    ctx->gpr[21] = spu_ai(ctx->gpr[21], 16);
    ctx->gpr[22] = spu_ai(ctx->gpr[22], 16);
    ctx->gpr[18] = spu_ai(ctx->gpr[18], 1);
    ctx->gpr[19] = spu_ceq(ctx->gpr[18], ctx->gpr[1]);
    if (ctx->gpr[19]._u32[0] == 0) goto loc_fixture;
    { ctx->pc = 0x5E0; g_spu_trampoline_fn = job_00844E80_spu_func_000005E0; return; }
}

static unsigned random_state = 0x31415910;
static unsigned random_word(void) {
    random_state ^= random_state << 13; random_state ^= random_state >> 17; random_state ^= random_state << 5;
    return random_state;
}
int main(void) {
    spu_context* a = aligned_alloc(16, sizeof *a);
    spu_context* b = aligned_alloc(16, sizeof *b);
    assert(a && b);
    memset(a, 0, sizeof *a);
    for (unsigned i = 0; i < SPU_LS_SIZE; ++i) a->ls_store[i] = (unsigned char)random_word();
    g_spu_ls_probe = 0; g_spu_smc_watch = 0;
    for (unsigned n = 0; n < 5000; ++n) {
        for (unsigned r = 0; r < 128; ++r) for (unsigned k = 0; k < 4; ++k) a->gpr[r]._u32[k] = random_word();
        a->image_id = 100;
        a->gpr[0] = spu_splat_u32(0x5e0);
        a->gpr[1] = spu_splat_u32(1 + n % 7);
        a->gpr[21] = spu_splat_u32(0x800u + (n & 15));
        // Some runs deliberately alias the register bank, requiring fallback.
        const int alias = n % 3 == 0;
        a->gpr[22] = spu_splat_u32(alias ? 0x40 : 0x1000u + (n & 15));
        memcpy(b, a, sizeof *a);
        a->ls = alias ? (uint8_t*)a->gpr : a->ls_store;
        b->ls = alias ? (uint8_t*)b->gpr : b->ls_store;
        g_spu_ls_watch_n = n % 5 == 0;
        assert(ben10_vertex_cache_safe(b) == (!alias && !g_spu_ls_watch_n));
        unsigned before = watch_calls;
        ben10_vertex_uncached_kernel(a);
        unsigned observed = watch_calls - before;
        before = watch_calls;
        job_00844E80_spu_func_000002E0(b);
        assert(watch_calls - before == observed);
        assert(!memcmp(a->gpr, b->gpr, sizeof a->gpr));
        assert(!memcmp(a->ls_store, b->ls_store, SPU_LS_SIZE));
        assert(a->pc == b->pc && a->pc == 0x5e0);
    }
    g_spu_ls_watch_n = 0;
    b->ls = (uint8_t*)&b->ls;
    assert(!ben10_vertex_cache_safe(b));
    b->ls = b->ls_store; b->image_id = 101;
    assert(!ben10_vertex_cache_safe(b));
    free(a); free(b);
    puts("PASS vertex cache: 5000 complete-state replays, loop carries, LS aliases, diagnostics and live-outs");
}
