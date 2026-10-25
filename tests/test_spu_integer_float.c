/* Exact int32 conversion: truncation boundaries, zero, signs and other scales. */
#include "spu_helpers.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <xmmintrin.h>
static unsigned rng=0x1010abef;
static unsigned random_word(void) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static volatile unsigned checksum;
static __attribute__((noinline)) u128 fast(u128 a) {return spu_csflt(a,155);}
static __attribute__((noinline)) u128 reference(u128 a) {return spu_csflt_ref(a,155);}
static void check(u128 a,int scale)
{
    u128 x=spu_csflt(a,scale),y=spu_csflt_ref(a,scale);
    assert(!memcmp(&x,&y,16));
}
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double bench(u128 (*fn)(u128))
{
    u128 a={{0}};double start=now();
    for(unsigned n=0;n<2000000;++n) {a._u32[n&3]^=n+0x3120a8;a=fn(a);}
    checksum=a._u32[0];return (now()-start)*500.0;
}
int main(void)
{
    unsigned saved=_mm_getcsr();
    const int values[]={0,1,-1,INT_MIN,INT_MAX,16777215,16777216,16777217,-16777217,33554435,-33554435};
    for(unsigned mode=0;mode<4;++mode) {
        _mm_setcsr((saved&~0x6000u)|(mode<<13));
        for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i) {
            u128 a;for(unsigned k=0;k<4;++k)a._s32[k]=values[(i+k)%11];
            for(int scale=0;scale<256;++scale)check(a,scale);
        }
        for(unsigned n=0;n<500000;++n) {
            u128 a;for(unsigned k=0;k<4;++k)a._u32[k]=random_word();
            check(a,155);check(a,(int)n&255);
        }
    }
    _mm_setcsr(saved);
    printf("PASS CSFLT: 2000000 vectors, every scale, four rounding modes; SIMD %.2f ns, scalar %.2f ns\n",bench(fast),bench(reference));
}
