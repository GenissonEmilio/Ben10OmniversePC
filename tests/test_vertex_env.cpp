#include "vertex_float_env.h"
#include <xmmintrin.h>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
int main() {
    if(!ben10_vertex_float_supported()){puts("SKIP: AVX2/FMA3 unavailable");return 0;}
    unsigned original=_mm_getcsr();
    assert(ps3_host_spu_float_enter(101)==0);
    std::vector<std::thread> threads;
    for(unsigned i=0;i<4;++i)threads.emplace_back([i]{
        unsigned saved=0x1f80u|(i<<13)|(i&1?0x8040u:0);
        _mm_setcsr(saved);
        for(unsigned n=0;n<1000;++n){
            uint64_t token=ps3_host_spu_float_enter(100);
            assert(token && ben10_vertex_rtz_active==1);
            assert((_mm_getcsr()&0xffc0u)==0xffc0u);
            uint64_t nested=ps3_host_spu_float_enter(101);
            assert(nested && ben10_vertex_rtz_active==0);
            assert((_mm_getcsr()&0xe040u)==0);
            ps3_host_spu_float_leave(nested);
            assert(ben10_vertex_rtz_active==1 && (_mm_getcsr()&0xe040u)==0xe040u);
            uint64_t same=ps3_host_spu_float_enter(100);ps3_host_spu_float_leave(same);
            assert(ben10_vertex_rtz_active==1);
            ps3_host_spu_float_leave(token);
            assert(!ben10_vertex_rtz_active && _mm_getcsr()==saved);
        }
    });
    for(auto& thread:threads)thread.join();
    assert(!ben10_vertex_rtz_active && _mm_getcsr()==original);
    puts("PASS scoped FPU: nested jobs, four threads, exact restoration");
}
