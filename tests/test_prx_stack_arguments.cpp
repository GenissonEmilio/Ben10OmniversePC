/* Real production bridge with a synthetic thirteen-argument PRX export.
 * Test linkage scaffolding adapted from ps3recomp (MIT, see third_party).
 */
#include "ppu_context.h"
#include "ps3emu/guest_call.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

extern "C" {
uint8_t* vm_base;
uint32_t ppu_vm_size=0x10000;
uint32_t g_ww_lo=0,g_ww_hi=0;
int g_resv_store_active=0;
ps3_guest_caller_fn g_ps3_guest_caller=nullptr;
void ppu_hle_register_all();
int ps3_hle_has(uint32_t);
void ps3_hle_call(uint32_t,ppu_context*);
uint32_t prx_resolve_export(uint32_t nid);
void ps3_indirect_call(ppu_context*);
uint64_t ppu_guest_call(uint32_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t) { std::abort(); }
void ppu_prof_stamp(void*,unsigned) {}
void ppu_gcm_pump() {}
uint32_t ppu_prof_resolve_host(void*) { return 0; }
void ps3_msf(const char*,...) {}
int spu_coh_is_reserved(uint32_t) { return 0; }
void spu_coh_notify_write(uint32_t) {}
void spu_lockline_lock() {}
void spu_lockline_unlock() {}
void ppu_resv_break_store(uint64_t) {}
void ps3_ww_report_inline(uint32_t,uint64_t,int) {}
uint32_t vm_read32(uint64_t a) { uint32_t x;std::memcpy(&x,vm_base+a,4);return __builtin_bswap32(x); }
void vm_write32(uint64_t a,uint32_t x) { x=__builtin_bswap32(x);std::memcpy(vm_base+a,&x,4); }
}
extern "C" uint64_t vm_read64(uint64_t a) { uint64_t x;std::memcpy(&x,vm_base+a,8);return __builtin_bswap64(x); }
void vm_write64(uint64_t a,uint64_t x) { x=__builtin_bswap64(x);std::memcpy(vm_base+a,&x,8); }
extern "C" void ppu_dump_guest_stack(ppu_context*,const char*) {}
extern "C" void ppu_dump_bctrl_ring(uint32_t,const char*) {}
extern "C" void ppu_guest_callstack(const char*) {}
uint32_t g_ydkj_real_spurs_ea=0,g_ydkj_real_taskset_ea=0;


#include <cassert>
static constexpr uint32_t outer_nid=0xA0B01001, inner_nid=0xA0B01002;
static constexpr uint64_t caller_sp=0x8000, outer_toc=0x22334455, inner_toc=0x33445566;
static unsigned outer_calls, inner_calls;
extern "C" uint32_t prx_resolve_export(uint32_t nid) {
    return nid==outer_nid ? 0x1000 : nid==inner_nid ? 0x1010 : 0;
}
extern "C" void ps3_indirect_call(ppu_context* ctx) {
    const bool inner=ctx->ctr==0x2100;
    assert(ctx->ctr==(inner ? 0x2100u : 0x2000u));
    assert(ctx->gpr[1]==(inner ? caller_sp-0x100 : caller_sp));
    assert(ctx->gpr[2]==(inner ? inner_toc : outer_toc));
    for(unsigned i=0;i<8;++i)assert(ctx->gpr[i+3]==0xA000000000000000ull+i);
    // ELFv1 reserves all eight register slots; argument 9 starts at +0x70.
    for(unsigned i=0;i<5;++i)assert(vm_read64(ctx->gpr[1]+0x70+8*i)==0xB000000000000000ull+i);
    if(inner) { ++inner_calls;ctx->gpr[3]=0xFEDCBA9876543210ull;return; }
    ++outer_calls;
    // Model a real callee prologue and a nested import, including outgoing args.
    ctx->gpr[1]-=0x100;
    for(unsigned i=0;i<5;++i)vm_write64(ctx->gpr[1]+0x70+8*i,0xB000000000000000ull+i);
    ps3_hle_call(inner_nid,ctx);
    assert(ctx->gpr[1]==caller_sp-0x100);
    assert(ctx->gpr[2]==outer_toc);
    assert(vm_read64(ctx->gpr[1]+0x28)==outer_toc);
    ctx->gpr[1]+=0x100;
}
int main() {
    static uint8_t memory[0x10000];vm_base=memory;std::memset(memory,0xA5,sizeof memory);
    vm_write32(0x1000,0x2000);vm_write32(0x1004,outer_toc);
    vm_write32(0x1010,0x2100);vm_write32(0x1014,inner_toc);
    ppu_context ctx{};ctx.gpr[1]=caller_sp;ctx.gpr[2]=0x11223344;
    for(unsigned i=0;i<8;++i)ctx.gpr[i+3]=0xA000000000000000ull+i;
    for(unsigned i=0;i<5;++i)vm_write64(caller_sp+0x70+8*i,0xB000000000000000ull+i);
    ps3_hle_call(outer_nid,&ctx);
    assert(outer_calls==1 && inner_calls==1);
    assert(ctx.gpr[1]==caller_sp && ctx.gpr[2]==0x11223344);
    assert(ctx.gpr[3]==0xFEDCBA9876543210ull);
    assert(vm_read64(caller_sp+0x28)==0x11223344);
    assert(memory[caller_sp+0x98]==0xA5 && memory[caller_sp-0x108]==0xA5);
    for(unsigned i=0;i<5;++i)assert(vm_read64(caller_sp+0x70+8*i)==0xB000000000000000ull+i);
    std::puts("PASS: real PRX bridge preserves thirteen arguments, nested frames, TOCs, return value and canaries");
}
