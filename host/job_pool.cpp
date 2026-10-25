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

namespace {
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
            job.execute(job.address, job.index, job.size);
            job.done(job.chain, job.index);
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
        changed.wait(lock, [&]{ return pending.find(chain) == pending.end(); });
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
