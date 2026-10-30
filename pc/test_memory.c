/* The 32-bit runtime's memory: RAM and scratchpad at their PS1 addresses, the exe loaded into RAM, and the accessors
 * recompiled code goes through. usage: test_memory SCUS_941.63 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"

static unsigned int io_last_addr;
static int io_reads, io_writes;

unsigned int io_read(unsigned int addr, int size)
{
    (void)size;
    io_last_addr = addr;
    io_reads++;
    return 0x1234;
}

void io_write(unsigned int addr, unsigned int v, int size)
{
    (void)v;
    (void)size;
    io_last_addr = addr;
    io_writes++;
}

int main(int argc, char **argv)
{
    unsigned char header[0x800 + 64];
    FILE *f;
    int x = 0;

    assert(argc == 2);
    assert(mem_map() == 0);

    assert(exe_load(argv[1]) == 0x800110C0);
    f = fopen(argv[1], "rb");
    assert(f && fread(header, 1, sizeof(header), f) == sizeof(header));
    fclose(f);
    assert(!memcmp((void *)0x80010000, header + 0x800, 64));

    mem_w32(0xA0001000, 7);
    assert(mem_r32(0x80001000) == 7);
    /* null_page_is_kuseg_ram: the field reads through a null pointer, which the PS1 takes as RAM from 0 */
    assert(mem_r16(0x0000100A) == 7 >> 16 && mem_r32(0x00001000) == 7);
    mem_w8(0x0000FFFF, 9);
    assert(mem_r8(0x8000FFFF) == 9);
    /* A C call from code whose $sp is the top of RAM reads four words above it as stack arguments */
    assert(mem_r32(RAM_START + RAM_SIZE + 0x1C) == 0);
    mem_w8(0x1F800010, 0xAB); /* the scratchpad */
    assert(mem_r8(0x1F800010) == 0xAB);

    /* native_pointer_passthrough: C hands recompiled code the address of a native local */
    mem_w32((unsigned int)&x, 42);
    assert(x == 42);
    assert(mem_r16((unsigned int)&x) == 42);
    assert(mem_r32(0x1F801814) == 0x1234 && io_last_addr == 0x1F801814 && io_reads == 1);
    mem_w32(0x1F801810, 1);
    assert(io_last_addr == 0x1F801810 && io_writes == 1);
    puts("ok");
    return 0;
}
