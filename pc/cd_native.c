/* The CD layer on the PC: cd.c runs unchanged on top of a libcd that reads the extracted discs by sector.
 *
 * The drive always holds the disc the save is on (its disc number at 0x8009D588; disc 1 before there's a save), so
 * \MINT\DISKINFO.CNF always matches and main never runs the disc-change screen (FIELD/DSCHANGE.X). The PS1 build
 * keeps the check. Reads finish at once and the drive is always ready, so the retry paths never run.
 *
 * Game variables (cd_request, the state table) sit at their own addresses in the PC build's RAM; the state table's
 * code pointers are the runtime's to patch, like every other. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "cd.h"
#include "cd_native.h"

#define SAVE_DISC ((unsigned char *)0x8009D588) /* the save's disc number (main.c's D_8009D274.disc) */
#define SECTOR 2048

/* The hand-written decoder's state block (0x80034CF0) is replaced below, so its first word, the input pointer
 * cd.c sets, is a plain variable here. */
void *lzs_stream_src;

static struct {
    char folder[512];
    PcFile *files; /* by first sector */
    int count;
} discs[4];

static unsigned int position; /* the next sector to read */
static FILE *open_file;
static const PcFile *open_entry;

static int by_lba(const void *a, const void *b)
{
    const PcFile *x = a, *y = b;

    return x->lba < y->lba ? -1 : x->lba > y->lba;
}

static void load_disc(const char *folder)
{
    char path[600];
    unsigned char info[8];
    FILE *f;
    PcFile file;
    int disc, room = 0;

    snprintf(path, sizeof(path), "%s/MINT/DISKINFO.CNF", folder);
    if (!(f = fopen(path, "rb")))
        return;
    disc = fread(info, 1, 8, f) == 8 ? info[7] - '0' : 0;
    fclose(f);
    snprintf(path, sizeof(path), "%s/files.txt", folder);
    if (disc < 1 || disc > 3 || !(f = fopen(path, "r")))
        return;
    snprintf(discs[disc].folder, sizeof(discs[disc].folder), "%s", folder);
    while (fscanf(f, "%u %u %d %63s", &file.lba, &file.size, &file.raw, file.path) == 4) {
        if (discs[disc].count == room) {
            room = room ? room * 2 : 4096;
            discs[disc].files = realloc(discs[disc].files, room * sizeof(PcFile));
        }
        discs[disc].files[discs[disc].count++] = file;
    }
    fclose(f);
    qsort(discs[disc].files, discs[disc].count, sizeof(PcFile), by_lba);
}

int pc_cd_open(const char *assets)
{
    char folder[512];
    struct dirent *entry;
    DIR *dir = opendir(assets);
    int i, found = 0;

    if (!dir)
        return 0;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(folder, sizeof(folder), "%s/%s", assets, entry->d_name);
        load_disc(folder);
    }
    closedir(dir);
    for (i = 1; i <= 3; i++)
        found += discs[i].count > 0;
    return found;
}

const PcFile *pc_cd_files(int disc, int *count)
{
    *count = discs[disc].count;
    return discs[disc].files;
}

const char *pc_cd_folder(int disc)
{
    return discs[disc].folder;
}

static int inserted_disc(void)
{
    int disc = *SAVE_DISC;

    return disc >= 1 && disc <= 3 && discs[disc].count ? disc : 1;
}

static const PcFile *file_at(int disc, unsigned int lba)
{
    int lo = 0, hi = discs[disc].count - 1, mid;
    const PcFile *f;

    while (lo <= hi) {
        mid = (lo + hi) / 2;
        f = &discs[disc].files[mid];
        if (lba < f->lba)
            hi = mid - 1;
        else if (lba >= f->lba + (f->size + SECTOR - 1) / SECTOR)
            lo = mid + 1;
        else
            return f;
    }
    return 0;
}

/* One sector's 2048 data bytes. Past a file's end and outside every file it reads zeros, as the disc's padding does. */
static void read_sector(unsigned int lba, unsigned char *dst)
{
    int disc = inserted_disc();
    const PcFile *f = file_at(disc, lba);
    char path[600];
    size_t got = 0;
    long n;

    if (f && f != open_entry) {
        if (open_file)
            fclose(open_file);
        snprintf(path, sizeof(path), "%s/%s", discs[disc].folder, f->path);
        open_file = fopen(path, "rb");
        open_entry = f;
    }
    if (f && open_file) {
        n = lba - f->lba;
        fseek(open_file, f->raw ? n * 2352 + 24 : n * SECTOR, SEEK_SET);
        got = fread(dst, 1, SECTOR, open_file);
    }
    memset(dst + got, 0, SECTOR - got);
}

static int bcd(int n)
{
    return n / 10 * 16 + n % 10;
}

static unsigned int from_bcd(int n)
{
    return n / 16 * 10 + n % 16;
}

/* libcd, by the names cd.c knows it by. */
int CdInit(void) { return 1; }
int CdReset(int mode) { (void)mode; return 1; }
int CdIntToPos(int i, CdlLOC *p)
{
    i += 150; /* sector numbers start after the 2-second lead-in */
    p->minute = bcd(i / 4500);
    p->second = bcd(i / 75 % 60);
    p->sector = bcd(i % 75);
    return 1;
}

static int control(int com, void *param, unsigned char *result)
{
    CdlLOC *loc = param;

    if (com == CdlSetloc || com == CdlSeekL)
        position = (from_bcd(loc->minute) * 60 + from_bcd(loc->second)) * 75 + from_bcd(loc->sector) - 150;
    if (result) {
        memset(result, 0, 8);
        result[0] = CdlStatStandby; /* spinning, lid shut, no error */
    }
    return 1;
}

int func_8003DE9C(int com, void *param, void *result) { return control(com, param, result); }
int func_8003DFD4(int com, void *param) { return control(com, param, 0); }
int func_8003E100(int com, void *param, void *result) { return control(com, param, result); }
int func_8003DE2C(int mode, unsigned char *result) { (void)mode; (void)result; return CdlComplete; }
int CdDiskReady(int mode) { (void)mode; return CdlComplete; }
int func_8004076C(void) { return 2; } /* the disc type cd_check_disc accepts */
int func_8003E244(CdlATV *vol) { (void)vol; return 1; }
void func_8003DDA4(int mode) { (void)mode; }
void func_8003DD84(void) {}
void func_8003DE6C(int a) { (void)a; }
void func_8003DE84(int a) { (void)a; }

int CdRead(int sectors, void *buf, int mode)
{
    unsigned char *dst = buf;

    (void)mode;
    for (; sectors > 0; sectors--, dst += SECTOR)
        read_sector(position++, dst);
    return 1;
}

int CdReadSync(int mode, unsigned char *result) { (void)mode; (void)result; return 0; }

/* "\MINT\DISKINFO.CNF;1" on the inserted disc. */
CdlFILE *func_8003FB8C(CdlFILE *file, const char *name)
{
    int disc = inserted_disc();
    char path[64];
    int i, n = 0;

    for (name += *name == '\\'; *name && *name != ';' && n < 63; name++)
        path[n++] = *name == '\\' ? '/' : *name;
    path[n] = 0;
    for (i = 0; i < discs[disc].count; i++)
        if (!strcmp(discs[disc].files[i].path, path)) {
            CdIntToPos(discs[disc].files[i].lba, &file->pos);
            file->size = discs[disc].files[i].size;
            return file;
        }
    return 0;
}

/* The hand-written streaming decoder (0x80034CF0) in C: the LZS of lzs_decode, fed one 9-sector read
 * (lzs_stream_src) per call. Its packed length comes first; a reference's two bytes can straddle two reads. */
static unsigned char *lzs_out, *lzs_start;
static int lzs_left, lzs_flags, lzs_bits, lzs_low;

void lzs_stream_start(void *buf, void *dest)
{
    lzs_stream_src = buf;
    lzs_out = lzs_start = dest;
    lzs_left = -1;
}

/* Returns 0 once the file is unpacked, 1 when it needs the next read. */
int lzs_stream_resume(void)
{
    unsigned char *src = lzs_stream_src, *from, *to;
    int n = 9 * SECTOR, b;

    if (lzs_left < 0) {
        memcpy(&lzs_left, src, 4);
        src += 4;
        n -= 4;
        lzs_bits = 0;
        lzs_low = -1;
    }
    for (; n > 0; n--) {
        b = *src++;
        if (!lzs_bits) {
            lzs_flags = b;
            lzs_bits = 8;
            if (!--lzs_left)
                return 0;
            continue;
        }
        if (lzs_flags & 1) {
            *lzs_out++ = b;
            if (!--lzs_left)
                return 0;
        } else if (lzs_low < 0) {
            lzs_low = b;
            continue;
        } else {
            from = lzs_out - ((0xFEE - (lzs_low | (b & 0xF0) << 4) + (lzs_out - lzs_start)) & 0xFFF);
            to = lzs_out + (b & 0xF) + 3;
            for (; from < lzs_start; from++)
                *lzs_out++ = 0;
            while (lzs_out < to)
                *lzs_out++ = *from++;
            lzs_low = -1;
            if (!(lzs_left -= 2))
                return 0;
        }
        lzs_flags >>= 1;
        lzs_bits--;
    }
    return 1;
}
