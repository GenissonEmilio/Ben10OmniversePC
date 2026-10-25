/* Real cellPadGetData reports in BE guest memory, with no desktop focus. */
#include "cellPad.h"
#include "ppu_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

uint8_t* vm_base;
uint32_t ppu_vm_size;
int g_resv_store_active;
uint32_t g_ww_lo,g_ww_hi;
void ppu_resv_break_store(uint64_t ea) {(void)ea;}
void ps3_ww_report_inline(uint32_t a,uint64_t v,int w) {(void)a;(void)v;(void)w;}
int spu_coh_is_reserved(uint32_t a) {(void)a;return 0;}
void spu_coh_notify_write(uint32_t a) {(void)a;}
void spu_lockline_lock(void) {}
void spu_lockline_unlock(void) {}
uint32_t cellGcm_flip_request_count(void) {return 0;}

static char path[]="/tmp/ben10-pad-XXXXXX";
static void command(const char* text) {FILE* f=fopen(path,"w");assert(f);fputs(text,f);fclose(f);}
static unsigned word(unsigned offset) {return vm_read16(0x104+2*offset);}
static void poll(void) {assert(cellPadGetData(0,(void*)0x100)==0);assert(vm_read32(0x100)>0);}
int main(void)
{
    int fd=mkstemp(path);assert(fd>=0);close(fd);
    setenv("PAD_FILE",path,1);setenv("PAD_ALWAYS_CHANGE","1",1);
    setenv("PAD_NO_KEYBOARD","1",1);
    vm_base=calloc(1,0x10000);assert(vm_base);ppu_vm_size=0x10000;
    assert(cellPadInit(1)==0);
    command("0 3 128 0 255 128\n");
    for (int i=0;i<3;++i) {poll();assert(word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y)==0);assert(word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X)==255);}
    poll();assert(word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y)==128);assert(word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X)==128);
    command("0x4000 2\n");
    for (int i=0;i<2;++i) {poll();assert(word(CELL_PAD_BTN_OFFSET_DIGITAL2)==0x40);assert(word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X)==128);}
    poll();assert(word(CELL_PAD_BTN_OFFSET_DIGITAL2)==0);
    command("0x8000 2 255 128 128 128\n");
    for (int i=0;i<2;++i) {poll();assert(word(CELL_PAD_BTN_OFFSET_DIGITAL2)==0x80);assert(word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X)==255);}
    poll();assert(word(CELL_PAD_BTN_OFFSET_DIGITAL2)==0);assert(word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X)==128);
    command("0x4000 3 256 0 128 128\n");poll();assert(word(CELL_PAD_BTN_OFFSET_DIGITAL2)==0);
    command("0x4000 3 128 0\n");poll();assert(word(CELL_PAD_BTN_OFFSET_DIGITAL2)==0);
    assert(cellPadEnd()==0);unlink(path);free(vm_base);
    puts("PASS PAD_FILE: analog, digital, combined, hold, release, malformed input");
}
