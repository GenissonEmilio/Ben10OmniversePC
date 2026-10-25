/* Manufactured NV4097 windows: no game code or captures. */
#include "rsx_dispatch.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    static rsx_dispatch r;
    rsx_dispatch_init(&r, NULL);
    r.regs[0x0208 / 4] = 0x148; /* pitch, RGBA8, Z24S8 */
    r.regs[0x0200 / 4] = (1280u << 16) | 77u;
    r.regs[0x0204 / 4] = (720u << 16) | 33u;
    r.regs[0x020C / 4] = 10240;
    r.regs[0x021C / 4] = 2048;
    r.regs[0x0280 / 4] = 4096;
    r.regs[0x0284 / 4] = 8192;
    r.regs[0x022C / 4] = 10240;
    r.regs[0x0218 / 4] = 0x100000;
    r.regs[0x0288 / 4] = 0x200000;
    r.regs[0x028C / 4] = 0x300000;
    r.regs[0x0214 / 4] = 0x400000;
    rsx_dsp_surface s;
    rsx_dsp_get_surface(&r, &s);
    assert(s.color_offset[0] == 0 && s.zeta_offset == 0x400000);
    assert(s.clip_x == 77 && s.clip_y == 33);
    /* Two adjacent 1280-pixel windows in 2560-pixel rows. Local offset 0
     * is valid, so the window must apply there as well as nonzero bases. */
    r.regs[0x02B8 / 4] = 1280;
    rsx_dsp_get_surface(&r, &s);
    assert(s.color_offset[0] == 0x1400);
    assert(s.zeta_offset == 0x401400);
    r.regs[0x02B8 / 4] = (3u << 16) | 16u;
    rsx_dsp_get_surface(&r, &s);
    const unsigned base[] = {0,0x100000,0x200000,0x300000};
    const unsigned pitch[] = {10240,2048,4096,8192};
    for (unsigned i=0; i<4; ++i)
        assert(s.color_offset[i] == base[i] + 64 + 3*pitch[i]);
    assert(s.clip_w == 1280 && s.clip_h == 720);
    /* FP16, FP32 and AA change the horizontal byte stride; depth has its
     * own format and pitch. Repeated decoding must never accumulate it. */
    r.regs[0x0208 / 4] = 0x12B | (4u << 12); /* FP16, Z16, 2x2 */
    rsx_dsp_get_surface(&r, &s);
    assert(s.color_offset[0] == 16*2*8 + 3*10240);
    assert(s.zeta_offset == 0x400000 + 16*2*2 + 3*10240);
    rsx_dsp_get_surface(&r, &s);
    assert(s.color_offset[0] == 16*2*8 + 3*10240);
    r.regs[0x0208 / 4] = 0x14C;
    rsx_dsp_get_surface(&r, &s);
    assert(s.color_offset[0] == 16*16 + 3*10240);
    r.regs[0x02B8 / 4] = 0;
    rsx_dsp_get_surface(&r, &s);
    assert(s.color_offset[0] == 0);
    puts("PASS window offset: atlas, MRT, depth, AA, formats, reset");
}
