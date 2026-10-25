/* Reuse the SDK's independent exact-integer oracle and adversarial vectors. */
#include "vertex_float.h"
#include <assert.h>
#define main ben10_float_reference_suite
#include "../third_party/ps3recomp/runtime/spu/tests/test_spu_float_neon.c"
#undef main
int main(void) {
    if(!ben10_vertex_float_supported()){puts("SKIP: AVX2/FMA3 unavailable");return 0;}
    unsigned saved=_mm_getcsr();
    uint64_t token=ps3_host_spu_float_enter(100);
    assert(ben10_vertex_rtz_active);
    int result=ben10_float_reference_suite();
    ps3_host_spu_float_leave(token);
    assert(!ben10_vertex_rtz_active && _mm_getcsr()==saved);
#ifndef BEN10_VERTEX_ALWAYS_SCOPED
    /* The normal, unscoped path must retain the original exact behavior. */
    u128 a=spu_splat_u32(0x3f800000u),c=spu_splat_u32(0xa1800000u);
    u128 x=spu_fma(a,a,c),y=spu_fma_ref(a,a,c);
    assert(!memcmp(&x,&y,sizeof x));
#endif
    return result;
}
