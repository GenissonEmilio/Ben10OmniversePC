#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
extern thread_local int ben10_vertex_rtz_active;
#else
extern _Thread_local int ben10_vertex_rtz_active;
#endif
int ben10_vertex_float_supported(void);
int ben10_vertex_float_required(void);
uint64_t ps3_host_spu_float_enter(int image_id);
void ps3_host_spu_float_leave(uint64_t token);
#ifdef __cplusplus
}
#endif
