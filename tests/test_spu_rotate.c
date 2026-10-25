/* Compare the production SIMD helper to its independent scalar byte loop. */
#include "spu_helpers.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

static unsigned rng=0x2310abcd;
static unsigned random_word(void) { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static volatile unsigned checksum;
static __attribute__((noinline)) u128 fast(u128 a, u128 b) { return spu_rotqby(a,b); }
static __attribute__((noinline)) u128 reference(u128 a, u128 b) { return spu_rotqby_ref(a,b); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }
static double bench(u128 (*fn)(u128,u128))
{
    u128 a={{0}},b={{0}}; a._u32[0]=0x12345678; a._u32[1]=0xabcdef01;
    a._u32[2]=0x31415926; a._u32[3]=0x10203040;
    double start=now();
    for (unsigned i=0;i<2000000;++i) { b._u32[0]=i; a=fn(a,b); a._u32[i&3]^=i; }
    checksum=a._u32[0]; return (now()-start)*500.0;
}
int main(void)
{
    for (unsigned n=0;n<2000000;++n) {
        u128 a,b;
        for (unsigned i=0;i<4;++i) { a._u32[i]=random_word(); b._u32[i]=random_word(); }
        if (n<256) b._u32[0]=n;
        u128 x=fast(a,b), y=reference(a,b);
        assert(!memcmp(&x,&y,sizeof x));
    }
    printf("PASS SPU rotate: 2000000 differential vectors, SIMD %.2f ns, scalar %.2f ns\n",
           bench(fast),bench(reference));
}
