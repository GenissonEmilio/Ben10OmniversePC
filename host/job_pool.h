#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*ps3_host_job_fn)(uint32_t job, int index, uint32_t descriptor_size);
typedef void (*ps3_host_job_done_fn)(uint32_t chain, int index);
typedef void (*ps3_host_job_spu_fn)(uint32_t spu);
int ps3_host_job_submit(uint32_t chain, uint32_t job, int index,
                       uint32_t descriptor_size, ps3_host_job_fn execute,
                       ps3_host_job_done_fn done, ps3_host_job_spu_fn set_spu);
void ps3_host_job_drain(uint32_t chain);
int ps3_host_job_barrier(uint64_t command);
void ps3_host_job_report(void);
#ifdef __cplusplus
}
#endif
