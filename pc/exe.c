#include <stdio.h>
#include <string.h>
#include "memory.h"

/* A PS-X EXE: pc0 at 0x10, the load address and size at 0x18, the code and data from 0x800. Returns pc0, or 0 if the
 * file can't be read or doesn't fit in RAM. */
unsigned int exe_load(const char *path)
{
    unsigned char header[0x800];
    unsigned int pc0, load, size;
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;
    if (fread(header, 1, sizeof(header), f) != sizeof(header) || memcmp(header, "PS-X EXE", 8)) {
        fclose(f);
        return 0;
    }
    memcpy(&pc0, header + 0x10, 4);
    memcpy(&load, header + 0x18, 4);
    memcpy(&size, header + 0x1C, 4);
    if (load < RAM_START || size > RAM_START + RAM_SIZE - load || fread((void *)load, 1, size, f) != size)
        pc0 = 0;
    fclose(f);
    return pc0;
}
