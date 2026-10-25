/* Scheduler services adapted from ps3recomp's MIT taskset-layout fixture.
 * Production scheduling, signal consumption and wait are inserted unchanged.
 * Only host dispatch is observed; it models a peer delivering actual work. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "ps3emu/log_first.h"
typedef int32_t LONG;
static LONG InterlockedExchange(volatile LONG* p, LONG v) { return __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST); }
static void Sleep(unsigned ms) { (void)ms; }
uint8_t* vm_base;
uint32_t g_ydkj_real_taskset_ea, g_ydkj_real_taskid, g_barrier_sync_watch;
static pthread_mutex_t s_sig_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_sig_cv=PTHREAD_COND_INITIALIZER;
enum { CSTS_SIGNALLED=0x40, TASKSET=0x1000, ELF=0x5000 };
static unsigned dispatched, ids, released, acquired;
void spu_lockline_lock(void) {}
void spu_lockline_unlock(void) {}
void spu_coh_notify_write(uint32_t a) { (void)a; }
static void spu_serial_release(void) { ++released; }
static void spu_serial_acquire(void) { ++acquired; }
unsigned long long ps3_ms_now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (unsigned long long)t.tv_sec*1000+t.tv_nsec/1000000; }
static int spurs_bitset_test(uint32_t ea,uint32_t id) { return !!(vm_base[ea+id/8]&(0x80u>>(id%8))); }
static void spu_taskset2_set_exit(uint32_t a,uint32_t b,int c,uint32_t d) { (void)a;(void)b;(void)c;(void)d; }
static int spu_workload_dispatch_async(const uint8_t* image,uint32_t size,uint32_t context)
{
    assert(image==vm_base+ELF && size==100);
    assert(g_ydkj_real_taskset_ea==TASKSET);
    const uint32_t id=g_ydkj_real_taskid;
    assert(id==0 || id==2);assert(context==0x8000+id*0x100);
    ++dispatched;ids|=1u<<id;
    // A submitted peer produces a signal for the waiting current task, id 1.
    vm_base[TASKSET+CSTS_SIGNALLED]|=0x40;
    return 1;
}
/* PRODUCTION_FUNCTIONS */
static void be16(uint8_t* p,uint16_t v) { p[0]=v>>8;p[1]=v; }
static void be32(uint8_t* p,uint32_t v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
int main(void)
{
    vm_base=calloc(1,0x20000);assert(vm_base);
    uint8_t* image=vm_base+ELF;
    memcpy(image,"\177ELF",4);image[4]=1;image[5]=2;
    be16(image+0x12,23);be32(image+0x18,0x3000);be32(image+0x1C,52);
    be16(image+0x2A,32);be16(image+0x2C,1);
    be32(image+52,1);be32(image+56,84);be32(image+60,0x3000);
    be32(image+68,16);be32(image+72,16);be32(image+76,5);
    uint8_t* ts=vm_base+TASKSET;
    ts[0x20]=0xA0;ts[0x30]=0xE0; // current task 1, new tasks 0 and 2
    ts_running(TASKSET)[0]=0x40;
    for(unsigned id=0;id<3;++id) {
        be32(ts+0x80+48*id+0x14,ELF|id);
        be32(ts+0x80+48*id+0x1C,0x8000+id*0x100);
    }
    uint8_t info[144];memcpy(info,ts+0x80,sizeof info);
    assert(spu_taskset_wait_signal(TASKSET,1)==0);
    assert(dispatched==2 && ids==5);
    assert(ts[0x20]==0 && ts[0x30]==0xE0);
    assert(ts_running(TASKSET)[0]==0xE0);
    assert(!spurs_bitset_test(TASKSET+CSTS_SIGNALLED,1));
    assert(!memcmp(info,ts+0x80,sizeof info));
    // A later wait never resubmits already running peers.
    ts[CSTS_SIGNALLED]|=0x40;
    assert(spu_taskset_wait_signal(TASKSET,1)==0 && dispatched==2);
    assert(released==2 && acquired==2);
    free(vm_base);puts("PASS: pending peers run before wait, deliver work, retain current task, and never duplicate");
}
