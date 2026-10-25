#pragma once

#ifndef _WIN32
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>

/* Guest-pointer bridges recover addresses by truncating host pointers to u32.
 * Keep the base 4 GiB aligned. Anonymous pages stay lazy until touched; unlike
 * calloc this also provides page alignment required by the runtime's mprotect.
 */
static inline uint8_t* ben10_map_flat_vm(void)
{
    const uintptr_t alignment = UINT64_C(0x100000000);
    const size_t size = alignment + 0x10000u;
    const size_t reservation = size + alignment;
    uint8_t* raw = (uint8_t*)mmap(NULL, reservation, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) return NULL;
    uintptr_t aligned = ((uintptr_t)raw + alignment - 1u) & ~(alignment - 1u);
    size_t head = aligned - (uintptr_t)raw;
    size_t tail = reservation - head - size;
    if (head) munmap(raw, head);
    if (tail) munmap((void*)(aligned + size), tail);
    return (uint8_t*)aligned;
}
#endif
