/* Reuse the upstream manufactured VM/callback fixture with the production
 * decoder API. Only decoded frame input is manufactured; no game data. */
#define main upstream_vdec_test_main
#include "libs/codec/tests/test_vdec_decode.c"
#undef main

static void enqueue(VdecSlot* v, unsigned token)
{
    v->aus[token % VDEC_AU_RING] = (CellVdecAuInfo){
        .startAddr=0x100000 + token*32, .size=24,
        .pts=90000 + token*3000, .dts=90000 + token*3000, .userData=token};
    VdecFfPicture picture = {.width=4, .height=4, .token=token,
        .pict_type=1, .key=1, .frame_ticks=3000, .yuv=malloc(24)};
    CHECK(picture.yuv);
    memset(picture.yuv,(int)token,24);
    vdec_on_picture(v,&picture);
}

int main(void)
{
    vm_base=calloc(1,MEM_SIZE);CHECK(vm_base);ppu_vm_size=MEM_SIZE;
    unsigned h=open_decoder(CELL_VDEC_CODEC_TYPE_AVC);
    VdecSlot* v=&s_vdec[h];
    // GetPicItem only needs a live-decoder marker, never dereferences it.
    v->ff=(VdecFf*)(uintptr_t)1;
    set_format(3,0,0xFF);
    for(unsigned round=0;round<20;++round) {
        unsigned first=round*2+1;
        enqueue(v,first);enqueue(v,first+1);
        u32 addresses[2];
        for(unsigned i=0;i<2;++i) {
            CHECK(cellVdecGetPicItem(h,(const CellVdecPicItem**)(uintptr_t)G_ITEMPTR)==CELL_OK);
            addresses[i]=vm_read32(G_ITEMPTR);
            CHECK(vm_read64(addresses[i]+0x30)==first+i);
            CHECK(vm_read32(addresses[i]+8)==128);
            CHECK(vm_read64(addresses[i]+0x10)==90000+(first+i)*3000);
        }
        CHECK(addresses[0]!=addresses[1]);
        CHECK(vm_read64(addresses[0]+0x30)==first);
        // Both metadata records are consumed while both pixel frames remain.
        CHECK(cellVdecGetPicItem(h,(const CellVdecPicItem**)(uintptr_t)G_ITEMPTR)==(s32)CELL_VDEC_ERROR_EMPTY);
        CHECK(v->qCount==2);
        for(unsigned i=0;i<2;++i) {
            CHECK(cellVdecGetPicture(h,(const void*)(uintptr_t)G_FMT,(void*)(uintptr_t)G_PIC)==CELL_OK);
            for(unsigned p=0;p<24;++p)CHECK(vm_base[G_PIC+p]==first+i);
        }
        CHECK(v->qCount==0);
    }
    v->ff=NULL;CHECK(cellVdecClose(h)==CELL_OK);free(vm_base);
    puts("PASS independent metadata and pixel consumers, stable records, FIFO pixels and ring reuse");
}
