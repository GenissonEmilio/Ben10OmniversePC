/* Taskset arguments survive the real attribute-to-shared-memory path.
 * Fixture services adapted from the ps3recomp MIT layout tests. */

#include "../third_party/ps3recomp/libs/spurs/cellSpurs.c"
#include <assert.h>
uint8_t* vm_base;
uint32_t ppu_vm_size;
/* No SPU runs in this layout fixture; observer-only services are inert. */
int g_resv_store_active;
uint32_t g_ww_lo, g_ww_hi;
void ppu_resv_break_store(uint64_t ea) {(void)ea;}
int spu_coh_is_reserved(uint32_t ea) {(void)ea; return 0;}
void spu_lockline_lock(void) {}
void spu_lockline_unlock(void) {}
void spu_coh_notify_write(uint32_t ea) {(void)ea;}
void ps3_ww_report_inline(uint32_t ea,uint64_t value,int width) {(void)ea;(void)value;(void)width;}

int main(void) {
    vm_base=calloc(1,0x10000);assert(vm_base);ppu_vm_size=0x10000;s_inst[0].ea=0x1000;
    CellSpursTasksetAttribute* attr=(void*)(uintptr_t)0x2000;
    const uint64_t values[]={0xFEDCBA9876543210ull,0x000000001B5E2C00ull,0};
    for(unsigned i=0;i<3;++i) {
        const uint32_t ts=0x3000+i*0x2000;
        assert(_cellSpursTasksetAttributeInitialize(attr,1,0x360001,values[i],0,4)==0);
        assert(cellSpursCreateTasksetWithAttribute((void*)(uintptr_t)0x1000,(void*)(uintptr_t)ts,attr)==0);
        assert(vm_read64(ts+CSTS_ARGS)==values[i]);
        assert(vm_read64(ts+CSTS_SPURS)==0x1000);
        assert(vm_read32(ts+CSTS_SIZE_FIELD)==0x1900);
    }
    assert(vm_read64(0x3000+CSTS_ARGS)==values[0]);
    assert(vm_read64(0x5000+CSTS_ARGS)==values[1]);
    assert(cellSpursTasksetAttributeInitialize(attr)==0);
    assert(cellSpursCreateTasksetWithAttribute((void*)(uintptr_t)0x1000,(void*)(uintptr_t)0x7000,attr)==0);
    assert(vm_read64(0x7000+CSTS_ARGS)==0);
    free(vm_base);puts("PASS: 64-bit taskset argument, independent instances and reset delivered through actual HLE creation");
}
