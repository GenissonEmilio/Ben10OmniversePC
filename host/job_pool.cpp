// Optional host workers for independent jobs between SPURS barriers.
#include "job_pool.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <ctime>

namespace {
bool statistics_enabled() {
    static const bool enabled = std::getenv("BEN10_JOB_STATS") != nullptr;
    return enabled;
}
uint64_t wall_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint64_t cpu_ns() {
    timespec value{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value);
    return uint64_t(value.tv_sec)*1000000000u + value.tv_nsec;
}
std::atomic<uint64_t> measured_jobs{},job_wall{},job_cpu{},drain_wall{},drains{},empty_drains{};
std::atomic<uint64_t> commands[128]{};
struct Job {
    uint32_t chain, address;
    int index;
    uint32_t size;
    ps3_host_job_fn execute;
    ps3_host_job_done_fn done;
    ps3_host_job_spu_fn set_spu;
};
class Pool {
    std::mutex mutex;
    std::condition_variable ready, changed;
    std::deque<Job> queue;
    std::vector<std::thread> workers;
    std::map<uint32_t,size_t> pending;
    bool stopping = false;
    void run(uint32_t spu) {
        for (;;) {
            Job job;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&]{ return stopping || !queue.empty(); });
                if (stopping && queue.empty()) return;
                job = queue.front(); queue.pop_front(); changed.notify_all();
            }
            job.set_spu(spu);
            const bool measure = statistics_enabled();
            const uint64_t begin = measure ? wall_ns() : 0;
            const uint64_t cpu_begin = measure ? cpu_ns() : 0;
            job.execute(job.address, job.index, job.size);
            job.done(job.chain, job.index);
            if (measure) {
                ++measured_jobs;
                job_wall += wall_ns()-begin;
                job_cpu += cpu_ns()-cpu_begin;
            }
            {
                std::lock_guard lock(mutex);
                if (!--pending.at(job.chain)) pending.erase(job.chain);
            }
            changed.notify_all();
        }
    }
public:
    explicit Pool(unsigned count) {
        try {
            for (unsigned i=0; i<count; ++i) workers.emplace_back([this,i]{ run(i); });
        } catch (...) {
            { std::lock_guard lock(mutex); stopping=true; }
            ready.notify_all();
            for (auto& worker : workers) worker.join();
            throw;
        }
    }
    void submit(Job job) {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&]{ return queue.size() < 64; });
        auto& count=pending[job.chain];
        queue.push_back(job); ++count;
        lock.unlock(); ready.notify_one();
    }
    void drain(uint32_t chain) {
        std::unique_lock lock(mutex);
        const bool measure = statistics_enabled();
        const uint64_t begin = measure ? wall_ns() : 0;
        if (measure) { ++drains; if (pending.find(chain)==pending.end()) ++empty_drains; }
        changed.wait(lock, [&]{ return pending.find(chain) == pending.end(); });
        if (measure) drain_wall += wall_ns()-begin;
    }
};
unsigned worker_count() {
    static unsigned count=[] {
        const char* value=std::getenv("PS3_JOB_WORKERS");
        if (!value || !*value) return 1u;
        char* end=nullptr;
        long parsed=std::strtol(value,&end,10);
        return end && !*end && parsed>=1 && parsed<=4 ? unsigned(parsed) : 1u;
    }();
    return count;
}
Pool* get_pool(bool create) {
    static std::atomic<Pool*> pool{nullptr};
    static std::mutex initialize;
    auto* value=pool.load(std::memory_order_acquire);
    if (value || !create) return value;
    std::lock_guard lock(initialize);
    value=pool.load(std::memory_order_relaxed);
    if (!value) {
        // One set of virtual SPU IDs shared by every chain. Guest threads
        // can submit during exit(); the OS retires these process resources.
        value=new Pool(worker_count());
        pool.store(value,std::memory_order_release);
        std::fprintf(stderr,"[ben10-jobs] %u workers shared across job chains\n",worker_count());
    }
    return value;
}
}
extern "C" int ps3_host_job_submit(uint32_t chain,uint32_t job,int index,
                                    uint32_t size,ps3_host_job_fn execute,
                                    ps3_host_job_done_fn done,ps3_host_job_spu_fn set_spu) {
    if (worker_count()==1 || !execute || !done || !set_spu) return 0;
    // Once queued, exactly one worker owns execution and its completion.
    try { get_pool(true)->submit({chain,job,index,size,execute,done,set_spu}); return 1; }
    catch (...) {
        std::fputs("[ben10-jobs] worker allocation failed; stopping game\n",stderr);
        std::abort();
    }
}
extern "C" void ps3_host_job_drain(uint32_t chain) {
    if (worker_count()==1) return;
    if (auto* pool=get_pool(false)) pool->drain(chain);
}
extern "C" int ps3_host_job_barrier(uint64_t command) {
    const unsigned op=command&7u, ext=command&127u;
    if (statistics_enabled()) ++commands[ext];
    static const bool pipeline=[] {
        const char* value=std::getenv("BEN10_JOB_PIPELINE");
        return value && *value=='1';
    }();
    if (!pipeline) return op!=0 || command==0;
    // JOB, NOP, RESET_PC, NEXT and CALL only advance the command walker.
    // SYNC/LWSYNC, FLUSH, JOBLIST, GUARD, END, RET and ABORT keep their drain.
    return op==2 || op==5 || op==6 || op==7;
}
extern "C" void ps3_host_job_report(void) {
    if (!statistics_enabled()) return;
    const uint64_t jobs=measured_jobs.exchange(0), calls=drains.exchange(0);
    std::fprintf(stderr,"[ben10-jobs-profile] jobs=%llu work_wall_ms=%.2f work_cpu_ms=%.2f drains=%llu empty=%llu drain_ms=%.2f commands=",
        (unsigned long long)jobs,job_wall.exchange(0)/1000000.0,job_cpu.exchange(0)/1000000.0,
        (unsigned long long)calls,(unsigned long long)empty_drains.exchange(0),drain_wall.exchange(0)/1000000.0);
    for (unsigned i=0;i<128;++i) {
        uint64_t count=commands[i].exchange(0);
        if (count) std::fprintf(stderr,"%02x:%llu,",i,(unsigned long long)count);
    }
    std::fputc('\n',stderr);
}
