/* Exercise the boot mapping against the real runtime's page-commit API. */
#include "flat_vm.h"
#include "vm.h"
#include <stdio.h>

uint8_t* vm_base;
uint32_t ppu_vm_size;
uint32_t ppu_hle_inject_base = 0x20000000u;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)

int main(void)
{
    vm_base = ben10_map_flat_vm();
    CHECK(vm_base != NULL);
    CHECK(((uintptr_t)vm_base & UINT32_MAX) == 0);
    const uint32_t addresses[] = {0x10000000u, 0x50000000u, 0xDFF00000u, 0xFFFFF000u};
    for (unsigned i = 0; i < sizeof addresses / sizeof addresses[0]; ++i) {
        uint32_t ea = addresses[i];
        CHECK(vm_commit(ea, 0x1000) == CELL_OK);
        CHECK(vm_base[ea] == 0);
        vm_base[ea] = 0xA5;
        CHECK((uint32_t)(uintptr_t)(vm_base + ea) == ea);
        CHECK(*(uint8_t*)vm_to_host(ea) == 0xA5);
    }
    CHECK(munmap(vm_base, UINT64_C(0x100010000)) == 0);
    puts("PASS: aligned guest pointers and commits across the 32-bit VM");
    return 0;
}
