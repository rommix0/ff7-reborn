#ifndef PC_MEMORY_H
#define PC_MEMORY_H

/* The PC port's memory: the PS1's 2 MB of RAM sits at 0x80000000 and the scratchpad at 0x1F800000, so a game address
 * is a real pointer in this 32-bit program. Recompiled code reaches memory through these accessors, which also send
 * the I/O window to the emulated hardware and pass any other address (a native local, say) straight through. */

#define RAM_START 0x80000000u
#define RAM_SIZE 0x200000u
#define SCRATCHPAD_START 0x1F800000u
#define IO_START 0x1F801000u
#define IO_END 0x1F803000u
#define UNCACHED_START 0xA0000000u /* KSEG1, the same RAM without the cache */

int mem_map(void);
unsigned int exe_load(const char *path);
unsigned int io_read(unsigned int addr, int size);
void io_write(unsigned int addr, unsigned int v, int size);

static inline void *mem_ptr(unsigned int addr)
{
    if (addr >= UNCACHED_START && addr < UNCACHED_START + RAM_SIZE)
        addr -= UNCACHED_START - RAM_START;
    return (void *)addr;
}

static inline int is_io(unsigned int addr)
{
    return addr >= IO_START && addr < IO_END;
}

static inline unsigned int mem_r8(unsigned int addr)
{
    return is_io(addr) ? io_read(addr, 1) : *(unsigned char *)mem_ptr(addr);
}

static inline unsigned int mem_r16(unsigned int addr)
{
    return is_io(addr) ? io_read(addr, 2) : *(unsigned short *)mem_ptr(addr);
}

static inline unsigned int mem_r32(unsigned int addr)
{
    return is_io(addr) ? io_read(addr, 4) : *(unsigned int *)mem_ptr(addr);
}

static inline void mem_w8(unsigned int addr, unsigned int v)
{
    if (is_io(addr))
        io_write(addr, v, 1);
    else
        *(unsigned char *)mem_ptr(addr) = v;
}

static inline void mem_w16(unsigned int addr, unsigned int v)
{
    if (is_io(addr))
        io_write(addr, v, 2);
    else
        *(unsigned short *)mem_ptr(addr) = v;
}

static inline void mem_w32(unsigned int addr, unsigned int v)
{
    if (is_io(addr))
        io_write(addr, v, 4);
    else
        *(unsigned int *)mem_ptr(addr) = v;
}

#endif
