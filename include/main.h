#ifndef MAIN_H
#define MAIN_H

/* Where a file is on the disc. */
typedef struct DiscFile {
    unsigned int lba;
    unsigned int size;
} DiscFile;

extern DiscFile disc_files[]; /* by number */
/* Picks what runs next (func_800146A4). Shared with the VSync callback, hence volatile: the game reloads it and reads
 * it through lhu. */
extern volatile short game_mode;

void func_80033E34(unsigned int lba, unsigned int size, void *dest, void (*callback)(void)); /* starts a disc read */
int func_80034B44(void); /* a disc read is still running */
void read_disc_file(int i, void *dest, void (*callback)(void));
void wait_disc_read(void (*idle)(void));
void func_80015CA0(void *src, void *dest); /* unpacks a module */
void func_800140F4(void);
int DrawSync(int mode);

#endif
