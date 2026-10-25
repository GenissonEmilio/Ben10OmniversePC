/* A streamed codec must remain callable while the DSP swap slot changes.
 * Exercise the production DMA residency bookkeeping without game data. */
#include <assert.h>
#include "../third_party/ps3recomp/runtime/spu/spu_channels.c"

uint8_t* vm_base;
uint32_t ppu_vm_size;

int main(void)
{
    spu_context* ctx = calloc(1, sizeof(*ctx)); assert(ctx);
    ctx->ls = calloc(1, SPU_LS_SIZE); assert(ctx->ls);
    const uint8_t codec[16] = {1, 3, 5, 7};
    const uint8_t dsp[16] = {2, 4, 6, 8};
    spu_overlay_register_sig_region(codec, 0x8000, 300);
    spu_overlay_register_sig(dsp, 206);
    memcpy(ctx->ls + 0x33180, codec, 16);
    spu_overlay_note_get(ctx, 0x18000000, ctx->ls + 0x33180, 0x4000);
    assert(ctx->resident_code[0].image_id == 300);
    assert(ctx->resident_code[0].lsa == 0x33180);
    assert(ctx->resident_code[0].size == 0x8000);
    /* Continuation of the same DMA image preserves ownership. */
    spu_overlay_note_get(ctx, 0x18004000, ctx->ls + 0x37180, 0x4000);
    assert(ctx->resident_code[0].image_id == 300);
    memcpy(ctx->ls + 0x30080, dsp, 16);
    spu_overlay_note_get(ctx, 0x19000000, ctx->ls + 0x30080, 0x3000);
    assert(ctx->resident_ovl == 206);
    assert(ctx->resident_code[0].image_id == 300);
    /* Unrelated content overwriting even part of the codec evicts it. */
    spu_overlay_note_get(ctx, 0x1A000000, ctx->ls + 0x38000, 0x100);
    assert(!ctx->resident_code[0].image_id);
    assert(ctx->resident_ovl == 206);
    free(ctx->ls); free(ctx);
    puts("PASS: codec and DSP coexist; continuation retains, overwrite evicts");
    return 0;
}
