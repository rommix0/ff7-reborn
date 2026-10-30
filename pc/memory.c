#include "memory.h"

#ifdef _WIN32
#include <windows.h>

static int map_at(unsigned int addr, unsigned int size)
{
    return VirtualAlloc((void *)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == (void *)addr ? 0 : -1;
}
#else
#include <sys/mman.h>

static int map_at(unsigned int addr, unsigned int size)
{
    return mmap((void *)addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0)
        == (void *)addr ? 0 : -1;
}
#endif

int mem_map(void)
{
    /* The scratchpad is 1 KB; a page is the smallest mapping. The I/O window above it stays unmapped. The 64 KB after
     * RAM (where the PS1 mirrors it) is there for the stack-argument words a call from the top of the stack reads. */
    return map_at(RAM_START, RAM_SIZE) || map_at(RAM_START + RAM_SIZE, 0x10000) || map_at(SCRATCHPAD_START, 0x1000)
        ? -1 : 0;
}
