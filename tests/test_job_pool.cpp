#include "job_pool.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace std::chrono_literals;
static std::atomic<unsigned> entered,finished,active,maximum,spu_mask;
static std::atomic<bool> release_jobs;
static std::atomic<unsigned> results[1024], completions[1024];
static thread_local unsigned spu_id=99;
static unsigned expected_workers=2;
static void select_spu(uint32_t id) {spu_id=id; assert(id<expected_workers);}
static void execute(uint32_t job,int index,uint32_t size) {
    assert(job==unsigned(index)+0x10000 && size==128);
    spu_mask.fetch_or(1u<<spu_id);
    unsigned count=++active, prior=maximum.load();
    while(count>prior && !maximum.compare_exchange_weak(prior,count)) {}
    ++entered;
    while(!release_jobs.load()) std::this_thread::sleep_for(1ms);
    results[index]=job^0x55AAu;
    --active;
}
static void done(uint32_t chain,int index) {
    assert(chain==1 || chain==2);
    assert(results[index]==((unsigned(index)+0x10000u)^0x55AAu));
    assert(completions[index].fetch_add(1)==0);
    ++finished;
}
static void wait_entered(unsigned count) {
    auto until=std::chrono::steady_clock::now()+2s;
    while(entered<count && std::chrono::steady_clock::now()<until) std::this_thread::sleep_for(1ms);
    assert(entered>=count);
}
int main(int argc,char** argv) {
    bool disabled=argc>1 && !std::strcmp(argv[1],"disabled");
    expected_workers=argc>1 && !std::strcmp(argv[1],"four") ? 4 : 2;
    setenv("PS3_JOB_WORKERS",disabled?"1":expected_workers==4?"4":"2",1);
    if(disabled) {
        assert(!ps3_host_job_submit(1,0x10000,0,128,execute,done,select_spu));
        ps3_host_job_drain(1); assert(!entered && !finished);
        puts("PASS disabled job pool preserves serial fallback"); return 0;
    }
    for(int i=0;i<16;++i)
        assert(ps3_host_job_submit(1,0x10000+i,i,128,execute,done,select_spu));
    wait_entered(expected_workers);
    assert(active==expected_workers && maximum==expected_workers &&
           spu_mask==((1u<<expected_workers)-1) && !finished);
    release_jobs=true; ps3_host_job_drain(1);
    assert(finished==16 && !active);
    // Force backpressure and reuse while two host producers publish work.
    auto producer=[](int first) {
        for(int i=first;i<first+256;++i)
            assert(ps3_host_job_submit(1,0x10000+i,i,128,execute,done,select_spu));
    };
    std::thread a(producer,16),b(producer,272);a.join();b.join();
    ps3_host_job_drain(1); assert(finished==528 && !active);
    for(unsigned i=0;i<528;++i) assert(completions[i]==1);
    // A barrier belongs to its chain; other queued work is kept intact.
    release_jobs=false;
    assert(ps3_host_job_submit(2,0x10000+528,528,128,execute,done,select_spu));
    wait_entered(529); ps3_host_job_drain(1); assert(finished==528);
    release_jobs=true; ps3_host_job_drain(2); assert(finished==529);
    assert(!ps3_host_job_submit(1,0,0,128,nullptr,done,select_spu));
    puts("PASS job pool: parallelism, unique SPU IDs, completion, barriers, backpressure, producers, chain isolation");
}
