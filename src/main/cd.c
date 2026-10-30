#include "common.h"
#include "main.h"
#include "sound.h"
#include "cd.h"

/* The game's CD layer (0x80033A90-0x80034CF0): one request at a time, by sector. cd_start_request queues it and
 * cd_step runs the current state's handler; callers poll cd_step until it returns CD_IDLE. Read errors retry, and
 * after 16 in a row the drive is stopped and the game waits for a readable disc (cd_check_disc). The disc number
 * comes from \MINT\DISKINFO.CNF. GCC 2.6.3 and ASPSX 2.21, like main.c. */

extern void (*cd_state_handlers[])(void); /* by state */

extern CdlATV cd_volume;
extern unsigned char cd_sector_buf[]; /* 9 sectors */
extern unsigned int cd_lzs_lba;
extern unsigned int cd_lzs_sectors;
extern int cd_retries;
extern int cd_last_vsync, cd_wait_frames; /* a command that takes 3600 frames is retried */
/* The hand-written streaming decoder: lzs_stream_resume unpacks what lzs_stream_src holds and returns 0 when the file
 * is done. */
extern void *lzs_stream_src;
void lzs_stream_start(void *buf, void *dest);
int lzs_stream_resume(void);

int cd_start_request(int mode, unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_seek(unsigned int lba, void (*callback)(void));
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_pause(void);
void cd_cancel(void);
void cd_stop(void);
int cd_check_disc(void);
int cd_read_disc_number(void);
void cd_finish(void);
void cd_error_sound(int code);

extern unsigned char D_8009D787;
extern unsigned char D_8009CBDC[3], D_8009D391[3];
extern short D_8009D78A;
extern unsigned char D_800716D4, D_800716CC, D_80071E30, D_80071E34;
extern unsigned char D_8009D5F0, D_8009D2A4, D_8009D2A5, D_8009AC2D;
extern int D_8009D264, D_8009D268;
extern short D_8009AC4C, D_8009AC4E, D_8009AC50;

/* Clears the 0x500 bytes up to D_8009D787 and resets play time and a few flags. */
void func_80033A90(void)
{
    unsigned char *p;
    int i;

    for (i = 0x4FF, p = &D_8009D787; i >= 0; i--)
        *p-- = 0;
    for (i = 0; i < 3; i++) {
        D_8009CBDC[i] = 0xFF;
        D_8009D391[i] = 0xFF;
    }
    D_8009D78A = 1;
    D_800716D4 = 0;
    D_800716CC = 0;
    D_80071E30 = 0;
    D_8009CBDC[0] = 0;
    D_8009D391[0] = 0;
    D_8009D5F0 = 0xFF;
    D_8009D2A4 = 0xFF;
    D_8009D2A5 = 0xFF;
    D_8009D264 = 0;
    D_8009D268 = 0;
    D_8009AC4C = 0;
    D_8009AC4E = 0;
    D_8009AC50 = 0;
    D_8009AC2D = 0;
    D_80071E34 = 0;
}

void cd_init(void)
{
    while (!CdInit())
        ;
    cd_request.state = CD_IDLE;
    func_8003DDA4(0);
    movie_init();
    /* The game passes the mode itself where libcd wants a pointer to it. */
    func_8003E100(CdlSetmode, (void *)CdlModeSpeed, 0);
    VSync(3);
    cd_request.disc = cd_read_disc_number();
    movie_load_table();
}

void cd_shutdown(void)
{
    cd_cancel();
    while (cd_step())
        ;
    func_8003DD84();
    CdReset(0);
}

/* CD audio volume. In mono each CD channel goes to both outputs at half its level. */
void cd_set_volume(int left, int left_to_right, int right, int right_to_left)
{
    if (seq_song.output_mode & OUTPUT_STEREO) {
        cd_volume.val0 = left;
        cd_volume.val1 = left_to_right;
        cd_volume.val2 = right;
        cd_volume.val3 = right_to_left;
    } else {
        cd_volume.val0 = left / 2;
        cd_volume.val1 = left / 2;
        cd_volume.val2 = right / 2;
        cd_volume.val3 = right / 2;
    }
    func_8003E244(&cd_volume);
}

/* Waits for the current request (stopping a movie or a paused stream), then queues this one. */
int cd_start_request(int mode, unsigned int lba, unsigned int size, void *dest, void (*callback)(void))
{
    int state;

    do {
        state = cd_step();
        switch (state) {
        case CD_MOVIE_PLAY:
        case CD_MOVIE_START:
        case CD_MOVIE_READY:
            movie_stop();
            break;
        case CD_MOVIE_D:
            func_8003DE9C(CdlPause, 0, 0);
            break;
        }
    } while (state);
    CdIntToPos(lba, &cd_request.loc);
    cd_request.sectors = (size + 0x7FF) >> 11;
    cd_request.dest = dest;
    cd_request.callback = callback;
    cd_request.state = mode;
}

int cd_seek(unsigned int lba, void (*callback)(void))
{
    cd_start_request(CD_SEEK, lba, 0, 0, callback);
    return 0;
}

int cd_set_location(unsigned int lba)
{
    cd_start_request(CD_IDLE, lba, 0, 0, 0);
    while (!func_8003DE9C(CdlSetloc, &cd_request.loc, 0))
        ;
    return 0;
}

int cd_read(unsigned int lba, unsigned int size, void *dest, void (*callback)(void))
{
    cd_start_request(CD_READ, lba, size, dest, callback);
    return 0;
}

/* Reads an LZS file and unpacks it to dest while it streams in. */
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void))
{
    cd_start_request(CD_LZS, lba, size, dest, callback);
    cd_lzs_lba = lba;
    lzs_stream_start(cd_sector_buf, dest);
    return 0;
}

int cd_seek_wait(unsigned int lba, void (*callback)(void))
{
    while (cd_seek(lba, callback))
        ;
    while (cd_step())
        VSync(0);
    return 0;
}

int cd_read_wait(unsigned int lba, unsigned int size, void *dest, void (*callback)(void))
{
    while (cd_read(lba, size, dest, callback))
        ;
    while (cd_step())
        VSync(0);
    return 0;
}

int cd_read_lzs_wait(unsigned int lba, unsigned int size, void *dest, void (*callback)(void))
{
    while (cd_read_lzs(lba, size, dest, callback))
        ;
    while (cd_step())
        VSync(0);
    return 0;
}

int cd_pause(void)
{
    cd_request.sectors = 0;
    cd_request.dest = 0;
    cd_request.callback = 0;
    cd_request.state = CD_PAUSE;
    return cd_step();
}

void cd_cancel(void)
{
    switch (cd_request.state) {
    case CD_READ_START:
    case CD_READ_WAIT:
    case CD_LZS_READ:
    case CD_LZS_WAIT:
        func_8003DE6C(0);
        func_8003DE84(0);
        cd_pause();
        break;
    case CD_MOVIE_PLAY:
    case CD_MOVIE_START:
    case CD_MOVIE_READY:
        movie_stop();
        break;
    case CD_IDLE:
    case CD_STOPPED:
        break;
    /* Listed although they do what default does: the game's jump table runs to 18. */
    case CD_SEEK:
    case CD_SEEK_WAIT:
    case CD_READ:
    case CD_READ_SEEK_WAIT:
    case CD_LZS:
    case CD_LZS_SEEK_WAIT:
    case CD_UNUSED:
    case CD_FINISH:
    case CD_DONE:
    case CD_MOVIE_D:
    default:
        cd_pause();
        break;
    }
}

void cd_stop(void)
{
    func_8003E100(CdlSetmode, 0, 0);
    VSync(3);
    func_8003E100(CdlStop, 0, 0);
    cd_request.state = CD_STOPPED;
}

/* While stopped: spins the disc up and checks it. 0 when it reads and has a disc number; otherwise why not: 3 lid
 * open, 5 no disc or no spin-up within 10 s, 6 disc number 0, 1, 2 and 4 other read problems. */
int cd_check_disc(void)
{
    unsigned char result[8];
    CdlLOC loc;
    int n;

    if (cd_request.state == CD_STOPPED) {
        func_8003E100(CdlNop, 0, result);
        if (result[0] & CdlStatShellOpen)
            return 3;
        n = 600;
        func_8003E100(CdlStandby, 0, 0);
        do {
            VSync(0);
            if (--n == 0)
                return 5;
            func_8003E100(CdlNop, 0, result);
        } while (!(result[0] & CdlStatStandby));
        n = CdDiskReady(0);
        switch (n) {
        case CdlDiskError:
            return 2;
        case CdlComplete:
            break;
        case CdlStatShellOpen:
            return 3;
        default:
            return 1;
        }
        switch (func_8004076C()) {
        case 1:
            return 4;
        case 0:
            return 5;
        case 2:
            break;
        case CdlStatShellOpen:
            return 3;
        default:
            return 1;
        }
        /* Sector 23 is SYSTEM.CNF. */
        CdIntToPos(23, &loc);
        func_8003E100(CdlSeekL, &loc, result);
        if (result[0] & CdlStatError)
            return 1;
        if (result[1] & 0x40)
            return 1;
        func_8003E100(CdlSetmode, (void *)CdlModeSpeed, result);
        VSync(3);
        cd_request.state = CD_IDLE;
        cd_request.disc = cd_read_disc_number();
        switch (cd_request.disc) {
        case 0:
            cd_stop();
            return 6;
        case -1:
            cd_stop();
            return 1;
        }
        movie_load_table();
    }
    return 0;
}

/* Padded to the game's 24 bytes: the link packs .rodata back to back, so the next file's data starts where the
 * game's does. */
static const char diskinfo_path[24] = "\\MINT\\DISKINFO.CNF;1";

/* The file starts "DISK0001": the eighth character is the disc number. */
int cd_read_disc_number(void)
{
    CdlFILE file;
    int r;

    while (cd_step())
        ;
    do {
        switch ((int)func_8003FB8C(&file, diskinfo_path)) {
        case -1:
        case 0:
            return -1;
        }
        func_8003E100(CdlSetloc, &file.pos, 0);
        CdRead(1, cd_sector_buf, CdlModeSpeed);
        while ((r = CdReadSync(1, 0)) > 0)
            ;
    } while (r);
    return cd_sector_buf[7] - '0';
}

int get_disc_number(void)
{
    return cd_read_disc_number();
}

int cd_get_state(void)
{
    return cd_request.state;
}

void cd_state_idle(void) {
}

void cd_state_stopped(void) {
}

void cd_state_done(void)
{
    cd_request.state = CD_FINISH;
}

void cd_finish(void)
{
    void (*callback)(void) = cd_request.callback;

    cd_request.state = CD_IDLE;
    if (callback)
        callback();
}

/* The three seek waits below share a shape: on an error, retry the seek, and after 16 errors stop the drive and wait
 * for a readable disc. They hold the counters' addresses in pointers, as the game's code does. */
void cd_state_seek(void)
{
    func_8003DFD4(CdlSetloc, &cd_request.loc);
    cd_request.state = CD_SEEK_WAIT;
    cd_wait_frames = 0;
    cd_retries = 0;
}

void cd_state_seek_wait(void)
{
    int now;
    int *last;
    int *retries;

    switch (func_8003DE2C(1, 0)) {
    case CdlComplete:
        cd_request.state = CD_DONE;
        break;
    case CdlDiskError:
        retries = &cd_retries;
        if (++*retries >= 16) {
            *retries = 0;
            cd_stop();
            do
                cd_error_sound(3);
            while (cd_check_disc());
        }
        cd_request.state = CD_SEEK;
        break;
    default:
        now = VSync(-1);
        last = &cd_last_vsync;
        if (*last != now) {
            *last = now;
            if (++cd_wait_frames == 3600) {
                cd_request.state = CD_SEEK;
                cd_error_sound(3);
            }
        }
        break;
    }
}

void cd_state_read(void)
{
    func_8003DFD4(CdlSetloc, &cd_request.loc);
    cd_request.state = CD_READ_SEEK_WAIT;
    cd_wait_frames = 0;
    cd_retries = 0;
}

void cd_state_read_seek_wait(void)
{
    int now;
    int *last;
    int *retries;

    switch (func_8003DE2C(1, 0)) {
    case CdlComplete:
        cd_request.state = CD_READ_START;
        break;
    case CdlDiskError:
        retries = &cd_retries;
        if (++*retries >= 16) {
            *retries = 0;
            cd_stop();
            do
                cd_error_sound(3);
            while (cd_check_disc());
        }
        cd_request.state = CD_READ;
        break;
    default:
        now = VSync(-1);
        last = &cd_last_vsync;
        if (*last != now) {
            *last = now;
            if (++cd_wait_frames == 3600) {
                cd_request.state = CD_READ;
                cd_error_sound(3);
            }
        }
        break;
    }
}

void cd_state_read_start(void)
{
    if (!CdRead(cd_request.sectors, cd_request.dest, CdlModeSpeed)) {
        cd_request.state = CD_READ;
        cd_error_sound(0x10);
    } else
        cd_request.state = CD_READ_WAIT;
}

void cd_state_read_wait(void)
{
    switch (CdReadSync(1, 0)) {
    case 0:
        cd_request.state = CD_DONE;
        break;
    case -1:
        cd_request.state = CD_READ;
        cd_error_sound(3);
        break;
    }
}

void cd_state_lzs(void)
{
    func_8003DFD4(CdlSetloc, &cd_request.loc);
    cd_request.state = CD_LZS_SEEK_WAIT;
    cd_wait_frames = 0;
    cd_retries = 0;
}

void cd_state_lzs_seek_wait(void)
{
    int now;
    int *last;
    int *retries;

    switch (func_8003DE2C(1, 0)) {
    case CdlComplete:
        cd_request.state = CD_LZS_READ;
        break;
    case CdlDiskError:
        retries = &cd_retries;
        if (++*retries >= 16) {
            *retries = 0;
            cd_stop();
            do
                cd_error_sound(3);
            while (cd_check_disc());
        }
        cd_request.state = CD_LZS;
        break;
    default:
        now = VSync(-1);
        last = &cd_last_vsync;
        if (*last != now) {
            *last = now;
            if (++cd_wait_frames == 3600) {
                cd_request.state = CD_LZS;
                cd_error_sound(3);
            }
        }
        break;
    }
}

void cd_state_lzs_read(void)
{
    cd_lzs_sectors = cd_request.sectors;
    if (cd_lzs_sectors > 8)
        cd_lzs_sectors = 9;
    if (!CdRead(cd_lzs_sectors, cd_sector_buf, CdlModeSpeed)) {
        cd_request.state = CD_LZS;
        cd_error_sound(3);
    } else
        cd_request.state = CD_LZS_WAIT;
}

void cd_state_lzs_wait(void)
{
    unsigned int *lba;
    CdRequest *req;

    switch (CdReadSync(1, 0)) {
    case 0:
        lba = &cd_lzs_lba;
        req = &cd_request;
        lzs_stream_src = cd_sector_buf;
        cd_request.sectors -= 9;
        *lba += 9;
        if (!lzs_stream_resume()) {
            req->state = CD_DONE;
            break;
        }
        CdIntToPos(*lba, &req->loc);
        req->state = CD_LZS;
        break;
    case -1:
        CdIntToPos(cd_lzs_lba, &cd_request.loc);
        cd_request.state = CD_LZS;
        cd_error_sound(3);
        break;
    }
}

void cd_state_pause(void)
{
    func_8003DFD4(CdlPause, 0);
    cd_request.state = CD_PAUSE_WAIT;
    cd_wait_frames = 0;
}

void cd_state_pause_wait(void)
{
    int now;
    int *last;

    switch (func_8003DE2C(1, 0)) {
    case CdlComplete:
        cd_finish();
        break;
    case CdlDiskError:
        cd_request.state = CD_PAUSE;
        break;
    default:
        now = VSync(-1);
        last = &cd_last_vsync;
        if (*last != now) {
            *last = now;
            if (++cd_wait_frames == 3600) {
                cd_request.state = CD_PAUSE;
                cd_error_sound(3);
            }
        }
        break;
    }
}

/* Runs the current state's handler and returns the state after it (CD_IDLE once the request is done). */
int cd_step(void)
{
    CdRequest *req;

    if (cd_request.state > CD_PAUSE_WAIT)
        for (;;)
            ;
    req = &cd_request;
    cd_state_handlers[req->state]();
    return req->state;
}

/* Unpacks an LZS file in memory: a u32 packed size, then groups of a flag byte and 8 items, each a literal byte (flag
 * bit set) or a 12-bit offset into a 4 KiB window and a length of 3-18. */
void lzs_decode(unsigned char *src, unsigned char *dst)
{
    unsigned char *start;
    unsigned char *end;
    unsigned char *from;
    unsigned char *to;
    int bits = 0;
    int flags = 0;
    int lo;
    int hi;

    start = dst;
    end = src + *(int *)src + 4;
    src += 4;
    for (;;) {
        if (!bits) {
            if (src >= end)
                return;
            bits = 8;
            flags = *src++;
        }
        if (flags & 1) {
            if (src >= end)
                return;
            *dst++ = *src++;
        } else {
            if (src >= end)
                return;
            lo = *src++;
            hi = *src++;
            lo |= (hi & 0xF0) << 4;
            /* Window positions before the start of the output read as zeros. */
            from = dst - ((0xFEE - lo + (dst - start)) & 0xFFF);
            to = dst + (hi & 0xF) + 3;
            for (; from < start; from++)
                *dst++ = 0;
            while (dst < to)
                *dst++ = *from++;
        }
        flags >>= 1;
        bits--;
    }
}

/* Sends sound command 30 (a lasting effect) with `effect` and waits a second. */
void cd_error_sound(int effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
    VSync(60);
}
