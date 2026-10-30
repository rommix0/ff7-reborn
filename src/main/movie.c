#include "common.h"
#include "main.h"
#include "sound.h"
#include "cd.h"

/* The movie player (0x80034F3C-0x8003623C), built on libpress and libcd's streaming. movie_open looks a movie up in
 * \MINT\MOVIE_ID.BIN and carves its buffers out of the caller's work area. Type 0 streams an STR from the disc: the
 * CD layer's states 8-10 run it (cd.h), each step showing one frame while the next is VLC-decoded. Types 2 and 3 are
 * read into memory whole (a table of frame offsets, then MDEC bitstreams) and loop or play once.
 *
 * Each streamed frame opens with 40 bytes that movie_decode_next copies to movie_camera for the field; their last u16
 * switches the MDEC between 16- and 24-bit colour from the frame after. GCC 2.6.3 and ASPSX 2.21, like cd.c. */

/* libcd's sector header for streamed frames (layout from Sony's Libref). */
typedef struct StHEADER {
    unsigned short id, type, secCount, nSectors;
    unsigned int frameCount, frameSize;
    unsigned short width, height;
    unsigned int dummy1, dummy2;
    CdlLOC loc;
} StHEADER;

/* One entry of \MINT\MOVIE_ID.BIN, by movie number. */
typedef struct MovieEntry {
    unsigned int lba, size;
    unsigned short x, y; /* where the frame goes in the display buffer */
    unsigned short w, h; /* frame size (types 2 and 3), or the XA volume left and right (type 0) */
    unsigned short frames; /* the last frame */
    unsigned short type; /* 0 streamed, 1 none, 2 in memory and looping, 3 in memory once */
} MovieEntry;

typedef struct MovieCamera {
    unsigned char camera[38]; /* read by the field */
    unsigned short rgb24;
} MovieCamera;

typedef struct Movie {
    int x, y;
    int width, height;
    unsigned int frames;
    unsigned int *rle[2]; /* run-level codes: one feeds the MDEC while the next frame is decoded into the other */
    unsigned int *slice_buf; /* one 16-pixel column of decoded pixels */
    int rle_index;
    int ended;
    int rgb24;
    RECT slice; /* where the next column goes in VRAM */
    short base_x, base_y; /* the display buffer being drawn */
    short end_x, end_y; /* the frame's right and bottom edges in VRAM */
    unsigned short type;
} Movie;

extern Movie movie;
extern MovieEntry movie_table[];
extern StHEADER *movie_header; /* the current frame's */
extern StHEADER movie_start_header; /* stands in until the first frame arrives */
extern MovieCamera movie_camera;
extern MovieCamera *movie_camera_ptr;
extern DISPENV *movie_target; /* the buffer frames are decoded into */
extern DISPENV *movie_display; /* the buffer on screen */
extern int D_80075CFC; /* probably StCdIntrFlag */
extern int D_800965E4, D_8009A060; /* 1 while an in-memory movie plays: the field keeps its 30 fps */
extern unsigned int *movie_data; /* an in-memory movie */
extern int movie_depth_seen, movie_depth, movie_depth_pending;
extern int movie_fading; /* the music fade before the end has been sent */
extern unsigned int movie_frame, movie_last_frame;
extern volatile int movie_frame_done; /* set by the MDEC callback after the last column */

void func_80041F14(int mode);                          /* probably DecDCTReset */
void func_800420C0(unsigned int *buf, int size);       /* probably DecDCTout */
void func_80042144(void (*func)(void));                /* probably DecDCToutCallback */
void func_8003DBA0(void *ring, int sectors);           /* probably StSetRing */
void func_80044000(RECT *rect, unsigned int *p);       /* probably LoadImage */
void func_80040CA8(void);                              /* probably StCdInterrupt */
void func_80040C88(int mask, int start, int end);      /* probably StSetMask */
unsigned int *offset_table_entry(unsigned int *table, int i); /* hand-written: table + table[i] */
int StGetNext(unsigned int **addr, StHEADER **header);
void StFreeRing(unsigned int *base);
void StRingStatus(short *free_sectors, short *over_sectors);
int StGetBackloc(CdlLOC *loc);
void StUnSetRing(void);
void StSetStream(int mode, int start, int end, void (*func1)(void), void (*func2)(void));
int DecDCTvlc(unsigned int *bs, unsigned int *buf);
void DecDCTin(unsigned int *buf, int mode);

static void movie_on_ready(void);
static void movie_reread(CdlLOC *loc);
static unsigned int *movie_next_frame(void);
static int movie_decode_next(void);
static void movie_slice_done(void);
static void movie_wait_frame(void);
static void movie_slice_done_memory(void);

static const char movie_table_path[24] = "\\MINT\\MOVIE_ID.BIN;1";

void movie_init(void)
{
    func_80041F14(0);
}

void movie_load_table(void)
{
    CdlFILE file;
    int r;

    do {
        while (!func_8003FB8C(&file, movie_table_path))
            ;
        func_8003E100(CdlSetloc, &file.pos, 0);
        CdRead(1, movie_table, 0x80);
        while ((r = CdReadSync(1, 0)) > 0)
            ;
    } while (r);
}

/* Returns the end of the buffers it took from buf, or 0 if the CD layer is busy. */
char *movie_open(char *buf, int id)
{
    if (cd_request.state)
        return 0;
    snd_request.arg[0] = 30;
    snd_request.arg[1] = 0x7FFF;
    if (cd_request.disc == 1) {
        if (id == 0x26)
            snd_request.arg[1] = 0;
        else if (id == 0x31)
            snd_request.arg[1] = 0x1800;
    }
    snd_request.id = 0xC9; /* slide the CD volume up */
    snd_submit_request();
    func_80041F14(1);
    movie.rle_index = 0;
    movie.rgb24 = 0;
    movie.ended = 0;
    movie.type = movie_table[id].type;
    movie_start_header.id = 0;
    movie_start_header.width = 320;
    movie_start_header.height = 224;
    movie_header = &movie_start_header;
    movie_frame_done = 0;
    movie_start_header.type = 0;
    movie_start_header.secCount = 0;
    movie_start_header.nSectors = 0;
    movie_start_header.frameSize = 0;
    movie_start_header.dummy1 = 0;
    movie_start_header.dummy2 = 0;
    movie_start_header.frameCount = 0;
    movie_frame = 1;
    movie_last_frame = 0;
    switch (movie.type) {
    case 0:
        movie.x = movie_table[id].x;
        movie.y = movie_table[id].y;
        movie.frames = movie_table[id].frames;
        movie.rle[0] = (unsigned int *)buf;
        buf += 0x19B00;
        movie.rle[1] = (unsigned int *)buf;
        buf += 0x19B00;
        movie.slice_buf = (unsigned int *)buf;
        buf += 0x2D00;
        movie.width = 320;
        movie.height = 224;
        func_8003DBA0(buf, 36);
        buf += 36 * 2048;
        movie_camera_ptr = &movie_camera;
        func_80042144(movie_slice_done);
        StSetStream(1, 0, -1, 0, 0);
        cd_seek(movie_table[id].lba, movie_on_ready);
        cd_set_volume(movie_table[id].w, 0, movie_table[id].h, 0);
        D_800965E4 = 0;
        D_8009A060 = 0;
        return buf;
    case 1:
        return 0;
    case 2:
    case 3:
        movie.x = movie_table[id].x;
        movie.y = movie_table[id].y;
        movie.frames = movie_table[id].frames;
        movie.width = movie_table[id].w;
        movie.height = movie_table[id].h;
        movie_data = (unsigned int *)buf;
        buf += (movie_table[id].size + 3) / 4 * 4;
        movie.rle[0] = (unsigned int *)buf;
        buf += 0xC800;
        movie.rle[1] = (unsigned int *)buf;
        buf += 0xC800;
        movie.slice_buf = (unsigned int *)buf;
        func_80042144(movie_slice_done_memory);
        buf += 0x1C00;
        cd_set_volume(255, 0, 255, 0);
        StSetStream(0, 0, -1, 0, 0);
        cd_read(movie_table[id].lba, movie_table[id].size, movie_data, movie_on_ready);
        D_800965E4 = 1;
        D_8009A060 = 1;
        break;
    }
    return buf;
}

/* CD state 9: the stream has started; wait for 8 sectors before the first frame. */
void movie_poll_start(void)
{
    short free_sectors, over_sectors;

    if (cd_request.state == CD_MOVIE_START) {
        if (movie.type || (StRingStatus(&free_sectors, &over_sectors), over_sectors >= 8)) {
            cd_request.state = CD_MOVIE_READY;
            movie_display->isrgb24 = 0;
            movie_depth_pending = 0;
            movie_depth = 0;
            movie_depth_seen = 0;
            movie.rgb24 = 0;
        }
    }
}

/* Decodes the first frame and lets the CD layer play the rest. Returns the CD state. */
int movie_start(void)
{
    unsigned int *frame;
    unsigned int *camera;
    unsigned int i;

    if (cd_request.state == CD_MOVIE_READY) {
        if (!movie.type) {
            for (i = 0x800000; !(frame = movie_next_frame());)
                if (!--i)
                    return 0;
            for (i = 0, camera = (unsigned int *)&movie_camera; i < 10; i++)
                *camera++ = *frame++;
            if (movie_depth_pending) {
                movie_depth_pending = 0;
                movie.rgb24 = movie_depth;
            } else if (movie_depth_seen != movie_camera.rgb24) {
                movie_depth = movie_depth_seen = movie_camera.rgb24;
                movie_depth_pending = 1;
            }
            DecDCTvlc(frame, movie.rle[movie.rle_index]);
            StFreeRing(frame);
        } else
            DecDCTvlc(offset_table_entry(movie_data, 0), movie.rle[movie.rle_index]);
        cd_request.state = CD_MOVIE_PLAY;
        movie_fading = 0;
    }
    return cd_request.state;
}

/* Inline, as the game has it: movie_play_frame carries two copies. */
inline void movie_stop(void)
{
    RECT clear;

    switch (cd_request.state) {
    case CD_MOVIE_PLAY:
    case CD_MOVIE_START:
    case CD_MOVIE_READY:
        func_8003DE9C(CdlPause, 0, 0);
        if (!movie.type)
            StUnSetRing();
        func_80042144(0);
        cd_request.state = CD_IDLE;
        D_800965E4 = 0;
        D_8009A060 = 0;
        if (movie_display->isrgb24) {
            clear.x = 0;
            clear.y = 0;
            clear.w = movie_target->disp.w * 3 / 2;
            clear.h = 480;
            ClearImage(&clear, 0, 0, 0);
            DrawSync(0);
        }
    }
}

/* CD state 8: send the decoded frame to the MDEC and decode the next one while it draws. */
void movie_play_frame(void)
{
    CdlLOC loc;

    movie_display->isrgb24 = movie_depth;
    movie.base_x = movie_target->disp.x;
    movie.base_y = movie_target->disp.y;
    movie.end_x = movie_target->disp.x + movie.width;
    movie.end_y = movie_target->disp.y + movie.height;
    movie.slice.x = movie.base_x + movie.x;
    movie.slice.y = movie.base_y + movie.y;
    movie.slice.h = movie.height;
    if (!movie.rgb24)
        movie.slice.w = 16;
    else {
        movie.slice.w = 24;
        movie.end_x = movie.end_x * 3 / 2;
    }
    switch (movie.type) {
    case 0:
        DecDCTin(movie.rle[movie.rle_index], movie.rgb24);
        func_800420C0(movie.slice_buf, movie.slice.w * movie.slice.h / 2);
        while (!movie_decode_next()) {
            func_80040C88(1, StGetBackloc(&loc), -1);
            movie_reread(&loc);
        }
        movie_wait_frame();
        if (movie.ended == 1)
            movie_stop();
        if (!movie_fading && movie_header->frameCount >= movie.frames - 15) {
            snd_request.id = 0xC9; /* slide the CD volume down */
            snd_request.arg[0] = 30;
            snd_request.arg[1] = 0;
            snd_submit_request();
            movie_fading = 1;
        }
        break;
    case 2:
        DecDCTin(movie.rle[movie.rle_index], 2);
        func_800420C0(movie.slice_buf, movie.slice.w * movie.slice.h / 2);
        movie.rle_index = !movie.rle_index;
        DecDCTvlc(offset_table_entry(movie_data, movie_frame), movie.rle[movie.rle_index]);
        if (movie_frame++ >= movie.frames)
            movie_frame = 0;
        movie_wait_frame();
        break;
    case 3:
        DecDCTin(movie.rle[movie.rle_index], 2);
        func_800420C0(movie.slice_buf, movie.slice.w * movie.slice.h / 2);
        movie.rle_index = !movie.rle_index;
        DecDCTvlc(offset_table_entry(movie_data, movie_frame), movie.rle[movie.rle_index]);
        if (movie_frame++ >= movie.frames)
            movie_stop();
        movie_wait_frame();
        break;
    }
}

/* The stream lost its place: seek back to it and restart. */
static void movie_reread(CdlLOC *loc)
{
    unsigned char mode[4];

    mode[0] = CdlModeSpeed;
    do {
        while (!func_8003DE9C(CdlSetloc, loc, 0))
            ;
        while (!func_8003DE9C(CdlSetmode, mode, 0))
            ;
        VSync(3);
    } while (!CdRead2(0x1E0));
}

/* The seek (type 0) or the whole read (types 2 and 3) has finished. */
static void movie_on_ready(void)
{
    cd_request.state = CD_MOVIE_START;
    if (!movie.type)
        while (!CdRead2(0x1E0))
            while (!func_8003DE9C(CdlSetloc, &cd_request.loc, 0))
                ;
}

/* The next streamed frame, or 0 if none comes in time. Flags the end, and follows changes of frame size. */
static unsigned int *movie_next_frame(void)
{
    unsigned int *addr;
    int n;

    for (n = 0x800000; StGetNext(&addr, &movie_header);)
        if (!--n)
            return 0;
    if (movie_header->frameCount >= movie.frames)
        movie.ended = 1;
    if (movie_last_frame >= movie_header->frameCount)
        movie.ended = 1;
    if (movie.width != movie_header->width || movie.height != movie_header->height) {
        movie.width = movie_header->width;
        movie.height = movie_header->height;
        if (!movie.rgb24) {
            movie.end_x = movie.width;
            movie.slice.h = movie.end_y = movie.height;
        } else {
            movie.end_x = movie.width * 3 / 2;
            movie.slice.h = movie.end_y = movie.height;
        }
    }
    movie_last_frame = movie_header->frameCount;
    return addr;
}

/* Takes the next frame's camera and colour depth, then VLC-decodes it into the free buffer. 0 if it didn't come. */
static int movie_decode_next(void)
{
    unsigned int *frame;
    unsigned int *camera;
    unsigned int i;

    for (i = 0x800000; !(frame = movie_next_frame());)
        if (!--i)
            return 0;
    for (i = 0, camera = (unsigned int *)&movie_camera; i < 10; i++)
        *camera++ = *frame++;
    if (movie_depth_pending) {
        movie_depth_pending = 0;
        movie.rgb24 = movie_depth;
    } else if (movie_depth_seen != movie_camera.rgb24) {
        movie_depth = movie_depth_seen = movie_camera.rgb24;
        movie_depth_pending = 1;
    }
    movie.rle_index = !movie.rle_index;
    DecDCTvlc(frame, movie.rle[movie.rle_index]);
    StFreeRing(frame);
    return 1;
}

/* The MDEC has decoded a column: copy it to VRAM and ask for the next, until the frame's right edge. */
static void movie_slice_done(void)
{
    if (D_80075CFC) {
        func_80040CA8();
        D_80075CFC = 0;
    }
    func_80044000(&movie.slice, movie.slice_buf);
    movie.slice.x += movie.slice.w;
    if (movie.slice.x < movie.end_x)
        func_800420C0(movie.slice_buf, movie.slice.w * movie.slice.h / 2);
    else
        movie_frame_done = 1;
}

static void movie_wait_frame(void)
{
    int n;

    if (cd_request.state == CD_MOVIE_PLAY) {
        n = 0x800000;
        while (!movie_frame_done) {
            HW_WAIT();
            if (!--n) {
                movie_frame_done = 1;
                movie.slice.x = movie.base_x + movie.x;
                movie.slice.y = movie.base_y + movie.y;
            }
        }
        movie_frame_done = 0;
    }
}

static void movie_slice_done_memory(void)
{
    func_80044000(&movie.slice, movie.slice_buf);
    movie.slice.x += movie.slice.w;
    if (movie.slice.x < movie.end_x)
        func_800420C0(movie.slice_buf, movie.slice.w * movie.slice.h / 2);
    else
        movie_frame_done = 1;
}
