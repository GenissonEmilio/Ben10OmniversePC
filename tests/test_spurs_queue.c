/* Real production queue operations, bounded producer/consumer interleavings. */
#include <assert.h>
#include <stdatomic.h>
#include "../third_party/ps3recomp/libs/spurs/cellSpurs.c"

uint8_t* vm_base;
uint32_t ppu_vm_size = 0x10000;
uint32_t g_ww_lo, g_ww_hi, g_spurs_job_ls_handle;
int g_resv_store_active;
static pthread_mutex_t line_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint notified, woke;
void spu_lockline_lock(void) { pthread_mutex_lock(&line_lock); }
void spu_lockline_unlock(void) { pthread_mutex_unlock(&line_lock); }
int spu_coh_is_reserved(uint32_t ea) { (void)ea; return 1; }
void spu_coh_notify_write(uint32_t ea) { (void)ea; atomic_fetch_add(&notified, 1); }
void ppu_resv_break_store(uint64_t ea) { (void)ea; }
void ps3_ww_report_inline(uint32_t a, uint64_t v, int w) { (void)a; (void)v; (void)w; }
void spu_taskset_signal_task(uint32_t taskset, uint32_t task)
{ assert(taskset == 0x5000); atomic_store(&woke, task + 1); }

static atomic_uint done;
static void* consumer(void* arg)
{
    unsigned count = (unsigned)(uintptr_t)arg;
    for (unsigned i = 1; i <= count; ++i) {
        assert(cellSpursQueuePopBody(0x1000, 0x4000, 0, 1) == CELL_OK);
        assert(vm_read32(0x4000) == i);
    }
    atomic_store(&done, 1); return NULL;
}
int main(void)
{
    vm_base = calloc(1, ppu_vm_size); assert(vm_base);
    assert(_cellSpursQueueInitialize(0, 0x5000, 0x1000, 0x2000, 16, 2, 2) == CELL_OK);
    // Literal BE wire layout consumed by the firmware SPU queue library:
    // pop, push, element size, depth, buffer u64, event port, direction.
    const uint8_t header[32] = {
        0,0,0,0, 0,0,0,0, 0,0,0,16, 0,0,0,2,
        0,0,0,0, 0,0,0x20,0, 0,0,0,0, 0,0,0,2};
    assert(!memcmp(vm_base + 0x1000, header, sizeof header));
    assert(cellSpursQueuePopBody(0x1000, 0x4000, 0, 0) == (s32)0x8041090Au);
    vm_write32(0x3000, 1);
    vm_base[0x1020] = 1; vm_base[0x1021] = 7;
    assert(cellSpursQueuePushBody(0x1000, 0x3000, 0) == CELL_OK);
    assert(atomic_load(&woke) == 8 && vm_base[0x1020] == 0);
    vm_write32(0x3000, 2);
    assert(cellSpursQueuePushBody(0x1000, 0x3000, 0) == CELL_OK);
    assert(cellSpursQueuePushBody(0x1000, 0x3000, 0) == (s32)0x8041090Au);
    assert(cellSpursQueuePopBody(0x1000, 0x4000, 1, 0) == CELL_OK);
    assert(vm_read32(0x4000) == 1 && vm_read32(0x1000) == 0);
    assert(cellSpursQueuePopBody(0x1000, 0x4000, 0, 0) == CELL_OK);
    assert(vm_read32(0x4000) == 1);
    assert(cellSpursQueuePopBody(0x1000, 0x4000, 0, 0) == CELL_OK);
    assert(vm_read32(0x4000) == 2);
    assert(atomic_load(&notified) >= 4);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, consumer, (void*)1000) == 0);
    Sleep(30); assert(!atomic_load(&done));
    for (unsigned i = 1; i <= 1000; ++i) {
        vm_write32(0x3000, i);
        assert(cellSpursQueuePushBody(0x1000, 0x3000, 1) == CELL_OK);
    }
    for (unsigned i = 0; i < 3000 && !atomic_load(&done); ++i) Sleep(1);
    assert(atomic_load(&done)); assert(pthread_join(thread, NULL) == 0);
    assert(cellSpursQueueSize(0x1000, 0x4500) == CELL_OK && vm_read32(0x4500) == 0);
    free(vm_base);
    puts("PASS: FIFO, peek, full/empty ABI, real copies, reservations, delayed consumer and 1000 wraps");
    return 0;
}
