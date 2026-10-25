/* Local-only replay of one captured vertex kernel. No game code/data here. */
#include "spu_context.h"
#include "vertex_float_env.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int g_spu_ls_watch_n = 0;
void spu_ls_watch_slow(uint32_t address, int write, const uint8_t* data,
                       uint32_t pc, uint32_t lr) { abort(); }
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
void job_00844E80_spu_func_000005E0(spu_context* ctx) {}
extern void job_00844E80_spu_func_000002E0(spu_context*);

static int read_exact(const char* path, void* data, size_t size) {
    FILE* input = fopen(path, "rb");
    if (!input) return 0;
    int ok = fread(data, 1, size, input) == size && fgetc(input) == EOF;
    fclose(input);
    return ok;
}
static uint64_t now_ns(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000000u + time.tv_nsec;
}
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    spu_context* source = aligned_alloc(16, sizeof(spu_context));
    spu_context* ctx = aligned_alloc(16, sizeof(spu_context));
    if (!source || !ctx) return 3;
    if (!read_exact(argv[1], source, sizeof *source) ||
        !read_exact(argv[2], source->ls_store, SPU_LS_SIZE)) return 4;
#ifdef VERTEX_FMA
    if (!ben10_vertex_float_supported()) return 5;
    uint64_t token = ps3_host_spu_float_enter(100);
#endif
    uint64_t begin = now_ns();
    for (unsigned n = 0; n < 1000; ++n) {
        memcpy(ctx, source, sizeof *ctx);
        ctx->ls = ctx->ls_store;
        job_00844E80_spu_func_000002E0(ctx);
    }
    uint64_t elapsed = now_ns() - begin;
#ifdef VERTEX_FMA
    ps3_host_spu_float_leave(token);
#endif
    FILE* output = fopen(argv[3], "wb");
    if (!output) return 6;
    int ok = fwrite(ctx->gpr, sizeof ctx->gpr, 1, output) == 1 &&
             fwrite(ctx->ls, SPU_LS_SIZE, 1, output) == 1;
    if (fclose(output)) ok = 0;
    printf("{\"microseconds\":%.3f,\"pc\":%u,\"iterations\":%u}\n",
        elapsed / 1000000.0, ctx->pc,
        ctx->gpr[40]._u32[0] - source->gpr[40]._u32[0]);
    free(source); free(ctx);
    return ok ? 0 : 7;
}
