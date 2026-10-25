/* A real ELFv1 callee spills register arguments into the caller's frame.
 * Keep a PROT_NONE guard immediately above the stack, as the runtime does. */
#include "ppu_context.h"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
int main(void)
{
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char* region = mmap(NULL, 3 * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(region != MAP_FAILED);
    assert(!mprotect(region + page, page, PROT_READ | PROT_WRITE));
    ppu_context ctx = {0};
    ppu_set_stack(&ctx, (uintptr_t)(region + page), page);
    const uintptr_t entry_sp = ctx.gpr[1];
    assert(!(entry_sp & 15));
    ctx.gpr[1] -= 0x150;
    *(uint64_t*)(uintptr_t)ctx.gpr[1] = entry_sp;
    // The first argument spill is entry_sp+48, not the callee's private frame.
    for (unsigned i = 0; i < 8; ++i) {
        const uintptr_t slot = ctx.gpr[1] + 0x150 + 48 + 8 * i;
        *(volatile uint64_t*)slot = 0x1234000000000000ull + i;
    }
    for (unsigned i = 0; i < 8; ++i)
        assert(*(uint64_t*)(entry_sp + 48 + 8 * i) == 0x1234000000000000ull + i);
    assert(entry_sp + 112 <= (uintptr_t)(region + 2 * page));
    assert(!munmap(region, 3 * page));
    puts("PASS: first PPU frame has all eight parameter slots below a protected stack boundary");
}
