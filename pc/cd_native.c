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
static CdlATV cd_mix = {0x80, 0, 0x80, 0}; /* CD audio into the SPU: left to left, left to right, right to right,
                                              right to left, 0x80 = as it is */
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

/* size bytes of a sector, from at bytes into its 2352 (its data starts at 24). A file kept as data alone has only
 * that; elsewhere, past a file's end and outside every file it reads zeros, as the disc's padding does. */
static void read_part(unsigned int lba, unsigned char *dst, int at, size_t size)
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
    if (f && open_file && (f->raw || at == 24)) {
        n = lba - f->lba;
        fseek(open_file, f->raw ? n * 2352 + at : n * SECTOR, SEEK_SET);
        got = fread(dst, 1, size, open_file);
    }
    memset(dst + got, 0, size - got);
}

static void read_sector(unsigned int lba, unsigned char *dst)
{
    read_part(lba, dst, 24, SECTOR);
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
int func_8003E244(CdlATV *vol) /* CdMix */
{
    cd_mix = *vol;
    return 1;
}
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

/* The hand-written streaming decoder (0x80034CF0) in C: the LZS of lzs_decode, 9 sectors of lzs_stream_src per call.
 * It leaves lzs_stream_src where it stopped: the CD layer points it at the next read each time, and ENDING.X, with
 * the whole file in memory, just calls again. Its packed length comes first; a reference's two bytes can straddle
 * two calls. */
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
    lzs_stream_src = src;
    return 1;
}

/* libcd's streaming, for the movies (movie.c). CdRead2 starts the drive streaming from its position at the
 * double-speed CD's 150 sectors a second of PS1 time; StGetNext hands out each frame once its last sector is in: its
 * video sectors' data joined after the first one's 32-byte header (libcd's StHEADER, its position in the last word),
 * in the ring buffer StSetRing gave, which holds two frames (one is VLC-decoded while the next comes in). The XA
 * audio sectors in between go to the SPU's CD input as the frames are taken. */
#define STR_VIDEO 0x80010160u /* a video sector's id and type */
#define STR_HEADER 32
#define SECTORS_PER_SECOND 150
#define RING_SECTORS 36
#define FRAME_ROOM (RING_SECTORS / 2 * SECTOR)
#define XA_GROUPS 18 /* of 128 bytes in an audio sector: 16 of unit headers, then 28 rows of 4 bytes */

double ps1_seconds(void);
void spu_cd_input(const short *lr, int frames, int rate);

static unsigned char *ring;
static int ring_half, streaming;
static unsigned int stream_first, stream_next; /* where the stream started, and its next sector */
static double stream_start;
static int xa_history[2][2];      /* each side's last two samples */
static double xa_emphasis[2][2];  /* each side's last sample in and out of the de-emphasis */

static short clamp16(double s)
{
    return s < -0x8000 ? -0x8000 : s > 0x7FFF ? 0x7FFF : (short)s;
}

/* An XA audio sector from its subheader on (psx-spx, "CDROM XA Audio ADPCM Compression"): each group 8 units of 28
 * 4-bit samples, or 4 of 8-bit ones (alternately left and right in stereo), at 37.8 or 18.9 kHz; through CdMix's
 * matrix to the SPU. A sector flagged for emphasis is undone with the CD's 50/15 us shelf (tools/movie.py works it the
 * same way). FF7's movies are all 4-bit. */
void xa_decode(const unsigned char *sector)
{
    static const int forward[4] = {0, 60, 115, 98}, back[4] = {0, 0, -52, -55};
    static short pcm[XA_GROUPS * 8 * 28][2];
    const unsigned char *group;
    int stereo, rate, units, frames, g, unit, j, shift, filter, s, side, i, l, r;
    int *h;

    if (!(sector[2] & 4) || (sector[3] >> 4 & 3) > 1) /* audio, 4- or 8-bit */
        return;
    stereo = sector[3] & 1;
    rate = sector[3] & 4 ? 18900 : 37800;
    units = sector[3] & 0x10 ? 4 : 8;
    frames = XA_GROUPS * 28 * units / (stereo ? 2 : 1);
    for (g = 0; g < XA_GROUPS; g++) {
        group = sector + 8 + g * 128;
        for (unit = 0; unit < units; unit++) {
            shift = group[4 + unit] & 0xF;
            shift = shift > 12 ? 9 : shift;
            filter = group[4 + unit] >> 4 & 3;
            side = stereo ? unit & 1 : 0;
            h = xa_history[side];
            for (j = 0; j < 28; j++) {
                s = units == 4 ? (short)(group[16 + j * 4 + unit] << 8)
                               : (short)((group[16 + j * 4 + unit / 2] >> (unit & 1) * 4 & 0xF) << 12);
                s = (s >> shift) + ((h[0] * forward[filter] + h[1] * back[filter] + 32) >> 6);
                h[1] = h[0];
                h[0] = clamp16(s);
                i = stereo ? (g * units / 2 + unit / 2) * 28 + j : (g * units + unit) * 28 + j;
                pcm[i][side] = h[0];
                if (!stereo)
                    pcm[i][1] = h[0];
            }
        }
    }
    for (i = 0; i < frames; i++) {
        if (sector[3] & 0x40) {
            double t50 = 2.0 * rate * 50e-6, t15 = 2.0 * rate * 15e-6;

            for (side = 0; side < 2; side++) {
                double *e = xa_emphasis[side], y = ((1 + t15) * pcm[i][side] + (1 - t15) * e[0] - (1 - t50) * e[1])
                    / (1 + t50);

                e[0] = pcm[i][side];
                e[1] = y;
                pcm[i][side] = clamp16(y);
            }
        }
        l = pcm[i][0], r = pcm[i][1];
        pcm[i][0] = clamp16((l * cd_mix.val0 + r * cd_mix.val3) >> 7);
        pcm[i][1] = clamp16((r * cd_mix.val2 + l * cd_mix.val1) >> 7);
    }
    spu_cd_input(pcm[0], frames, rate);
}

static void xa_sector(unsigned int lba)
{
    unsigned char sector[8 + XA_GROUPS * 128];

    read_part(lba, sector, 16, sizeof sector);
    xa_decode(sector);
}

static double sectors_in(void)
{
    return stream_first + (ps1_seconds() - stream_start) * SECTORS_PER_SECOND;
}

void func_8003DBA0(void *buf, int sectors) /* StSetRing */
{
    (void)sectors;
    ring = buf;
}

void StSetStream(int mode, int start, int end, void (*func1)(void), void (*func2)(void))
{
    (void)mode, (void)start, (void)end, (void)func1, (void)func2;
}

void func_80040C88(int mask, int start, int end) { (void)mask, (void)start, (void)end; } /* StSetMask */
void func_80040CA8(void) {} /* StCdInterrupt: the drive never interrupts the MDEC here */
void StFreeRing(unsigned int *base) { (void)base; }
void StUnSetRing(void) { streaming = 0; }

int CdRead2(int mode)
{
    (void)mode;
    stream_first = stream_next = position;
    stream_start = ps1_seconds();
    streaming = 1;
    memset(xa_history, 0, sizeof xa_history);
    memset(xa_emphasis, 0, sizeof xa_emphasis);
    return 1;
}

void StRingStatus(short *free_sectors, short *over_sectors)
{
    double in = streaming ? sectors_in() - stream_next : 0;

    *over_sectors = in < 0 ? 0 : in < RING_SECTORS ? (short)in : RING_SECTORS;
    *free_sectors = RING_SECTORS - *over_sectors;
}

int StGetBackloc(CdlLOC *loc)
{
    CdIntToPos(stream_next, loc);
    return 0;
}

/* 0 with the next frame, waiting for its sectors to come in; 1 when the stream has no more. */
int StGetNext(unsigned int **addr, unsigned int **header)
{
    unsigned char sector[SECTOR], *slot;
    unsigned int lba, id, frame = 0;
    unsigned short index, count = 0, k = 0;

    if (!streaming)
        return 1;
    slot = ring + (ring_half ^= 1) * FRAME_ROOM;
    for (lba = stream_next; !count || k < count; lba++) {
        if (lba - stream_next > 64)
            return 1;
        read_sector(lba, sector);
        memcpy(&id, sector, 4);
        memcpy(&index, sector + 4, 2);
        if (id != STR_VIDEO) {
            xa_sector(lba);
            continue;
        }
        if (!count && index)
            continue;
        if (!count) {
            memcpy(&count, sector + 6, 2);
            memcpy(&frame, sector + 8, 4);
            if (STR_HEADER + count * (SECTOR - STR_HEADER) > FRAME_ROOM)
                return 1;
            memcpy(slot, sector, STR_HEADER);
            CdIntToPos(lba, (CdlLOC *)(slot + 28));
        }
        if (index != k || memcmp(sector + 8, &frame, 4))
            return 1;
        memcpy(slot + STR_HEADER + k++ * (SECTOR - STR_HEADER), sector + STR_HEADER, SECTOR - STR_HEADER);
    }
    stream_next = lba;
    while (sectors_in() < stream_next)
        hw_poll();
    *addr = (unsigned int *)(slot + STR_HEADER);
    *header = (unsigned int *)slot;
    return 0;
}
