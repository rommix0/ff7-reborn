#ifndef CD_NATIVE_H
#define CD_NATIVE_H

/* One file of an extracted disc, from its files.txt (tools/extract_disc.py). */
typedef struct PcFile {
    unsigned int lba, size;
    int raw; /* kept as whole 2352-byte sectors (the movies with XA audio) */
    char path[64];
} PcFile;

/* Loads every disc folder under assets (found by its MINT/DISKINFO.CNF). Returns how many discs it found. */
int pc_cd_open(const char *assets);
const PcFile *pc_cd_files(int disc, int *count);
const char *pc_cd_folder(int disc);

#endif
