/* Synthetic producer/consumer test of the production job-chain walker. */
#include <assert.h>
#include <stdatomic.h>
#include "../third_party/ps3recomp/libs/spurs/cellSpurs.c"

uint8_t* vm_base;
uint32_t g_ww_lo, g_ww_hi, g_spurs_job_ls_handle;
int g_resv_store_active;
__thread uint32_t g_spurs_job_mbox, g_spurs_job_mbox_intr, g_spurs_job_cmd;
__thread int g_spurs_job_mbox_valid;
int spu_coh_is_reserved(uint32_t a) { (void)a; return 0; }
void spu_lockline_lock(void) { abort(); }
void spu_lockline_unlock(void) { abort(); }
void spu_coh_notify_write(uint32_t a) { (void)a; abort(); }
void ppu_resv_break_store(uint64_t a) { (void)a; abort(); }
void ps3_ww_report_inline(uint32_t a, uint64_t v, int w) { (void)a; (void)v; (void)w; abort(); }
int sys_event_queue_push_by_id(u32 id, u64 a, u64 b, u64 c, u64 d)
{ (void)id; (void)a; (void)b; (void)c; (void)d; abort(); }
static atomic_uint dispatched, finished;
static volatile LONG running;
int spu_workload_dispatch_job(const uint8_t* image, uint32_t size, uint32_t job, uint32_t desc)
{
    assert(image == vm_base + 0x8000 && size == 16 && job == 0x3000 && desc == 128);
    atomic_fetch_add(&dispatched, 1);
    return 0;
}
static void* consume(void* arg)
{
    (void)arg;
    jc_execute(0x1000, 0x2000, 128, &running);
    atomic_store(&finished, 1);
    return NULL;
}
static void join_bounded(pthread_t thread)
{
    for (unsigned i = 0; i < 2000 && !atomic_load(&finished); ++i) Sleep(1);
    assert(atomic_load(&finished));
    pthread_join(thread, NULL);
}
int main(void)
{
    vm_base = calloc(1, 0x10000); assert(vm_base);
    vm_write64(0x1000, UINT64_C(0x0000000800000012));
    vm_write64(0x1008, 0x7F); /* END */
    vm_write64(0x3000, 0x8000); vm_write16(0x3008, 1);
    pthread_t thread; running = 1;
    assert(pthread_create(&thread, NULL, consume, NULL) == 0);
    Sleep(80);
    assert(!atomic_load(&finished) && !atomic_load(&dispatched));
    vm_write64(0x1000, 0x3000); /* publish a real descriptor after the wait */
    join_bounded(thread); assert(atomic_load(&dispatched) == 1);
    vm_write64(0x1000, UINT64_C(0x0000000800000012));
    atomic_store(&finished, 0); running = 1;
    assert(pthread_create(&thread, NULL, consume, NULL) == 0);
    Sleep(20); _InterlockedExchange(&running, 0);
    join_bounded(thread); assert(atomic_load(&dispatched) == 1);
    free(vm_base);
    puts("PASS: delayed producer retained, one dispatch, parked queue shuts down");
    return 0;
}
