// Limit the SPU single-precision rounding environment to the vertex job thread.
#include "vertex_float_env.h"
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#define BEN10_X86 1
#endif
extern "C" { thread_local int ben10_vertex_rtz_active = 0; }
extern "C" int ben10_vertex_float_required(void) {
#ifdef BEN10_VERTEX_FMA
    return 1;
#else
    return 0;
#endif
}
extern "C" int ben10_vertex_float_supported(void) {
#if defined(BEN10_VERTEX_FMA) && defined(BEN10_X86)
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return 0;
#endif
}
extern "C" uint64_t ps3_host_spu_float_enter(int image_id) {
#ifdef BEN10_X86
    if (!ben10_vertex_float_supported()) return 0;
    // Other/nested jobs retain their normal host floating-point environment.
    const bool accelerated = image_id >= 100 && image_id <= 103;
    if (!accelerated && !ben10_vertex_rtz_active) return 0;
    const unsigned saved = _mm_getcsr();
    const uint64_t token = (1ull << 32) | (uint64_t(ben10_vertex_rtz_active) << 33) | saved;
    ben10_vertex_rtz_active = accelerated;
    // Mask exceptions, truncate, and flush denormals only within this job.
    _mm_setcsr(accelerated ? (saved & ~0x6000u) | 0xffc0u : (saved & ~0xe040u) | 0x1f80u);
    return token;
#else
    (void)image_id;
    return 0;
#endif
}
extern "C" void ps3_host_spu_float_leave(uint64_t token) {
#ifdef BEN10_X86
    if (!(token & (1ull << 32))) return;
    ben10_vertex_rtz_active = (token >> 33) & 1;
    _mm_setcsr((unsigned)token);
#else
    (void)token;
#endif
}
