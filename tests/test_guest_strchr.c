#include "../third_party/ps3recomp/libs/system/sysPrxForUser.c"
#include <assert.h>
uint8_t* vm_base;
uint32_t ppu_vm_size;
int main(void)
{
    vm_base = calloc(1, 1024); assert(vm_base);
    strcpy((char*)vm_base + 0x100, "x-movie://movie");
    assert((uintptr_t)_sys_strchr((void*)0x100, ':') == 0x107);
    assert((uintptr_t)_sys_strchr((void*)0x100, 0) == 0x10F);
    assert(!_sys_strchr((void*)0x100, '?'));
    assert(!_sys_strchr(NULL, ':'));
    free(vm_base);
    puts("PASS: strchr returns guest pointers, terminator, missing and null");
    return 0;
}
