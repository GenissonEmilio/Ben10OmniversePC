/* Independent byte-loop oracle, exhaustive selectors and mixed SIMD lanes. */
#ifndef BEN10_HELPERS_HEADER
#define BEN10_HELPERS_HEADER "spu_helpers.h"
#endif
#include BEN10_HELPERS_HEADER
#include <assert.h>
#include <stdio.h>
#include <time.h>

static unsigned state=0x15102026;
static unsigned random_word(void) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static volatile unsigned checksum;
static __attribute__((noinline)) u128 fast(u128 a,u128 b,u128 c) {return spu_shufb(a,b,c);}
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void check(u128 a,u128 b,u128 c)
{
    u128 actual=fast(a,b,c),expected=spu_shufb_ref(a,b,c);
    assert(!memcmp(&actual,&expected,sizeof actual));
}
int main(void)
{
    u128 a,b,c;
    for (unsigned i=0;i<16;++i) {a._u8[i]=i+16;b._u8[i]=i+128;c._u8[i]=i;}
    for (unsigned position=0;position<16;++position)
        for (unsigned selector=0;selector<256;++selector) {
            u128 control=c;control._u8[position]=selector;
            check(a,b,control);check(a,a,control);
        }
    for (unsigned n=0;n<2000000;++n) {
        for (unsigned i=0;i<4;++i) {
            a._u32[i]=random_word();b._u32[i]=random_word();c._u32[i]=random_word();
            if ((n&3)==1)c._u32[i]&=0x7f7f7f7f;
            if ((n&3)==2)c._u32[i]|=0x80808080;
        }
        check(a,(n&3)==3?a:b,c);
    }
    double start=now();
    for (unsigned n=0;n<4000000;++n) {
        a=fast(a,b,c);a._u32[n&3]^=n;c._u32[n&3]+=0x01010101;
    }
    checksum=a._u32[0];
    printf("PASS SHUFB: 8192 exhaustive cases, 2000000 mixed vectors; %.2f ns/op\n",(now()-start)*250.0);
}
