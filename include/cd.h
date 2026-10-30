#ifndef CD_H
#define CD_H

/* The CD layer's request state (cd.c) and the parts of libcd it and the movie player use. */

enum {
    CD_IDLE,
    CD_SEEK,
    CD_SEEK_WAIT,
    CD_READ, /* seek, then read */
    CD_READ_SEEK_WAIT,
    CD_READ_START,
    CD_READ_WAIT,
    CD_STOPPED,
    CD_MOVIE_PLAY, /* 8-10 are the movie player's (movie.c): each step shows a frame */
    CD_MOVIE_START, /* the stream is starting */
    CD_MOVIE_READY, /* buffered; waits for movie_start */
    CD_LZS, /* read 9 sectors at a time, unpacking as they arrive */
    CD_LZS_SEEK_WAIT,
    CD_LZS_READ,
    CD_LZS_WAIT,
    CD_UNUSED,
    CD_FINISH, /* go idle and call the request's callback */
    CD_DONE,
    CD_MOVIE_D, /* idle, but the next request pauses the drive first */
    CD_PAUSE,
    CD_PAUSE_WAIT
};

/* libcd's types, commands and results (names from Sony's libcd.h). */
typedef struct CdlLOC {
    unsigned char minute, second, sector, track;
} CdlLOC;

typedef struct CdlATV {
    unsigned char val0, val1, val2, val3;
} CdlATV;

typedef struct CdlFILE {
    CdlLOC pos;
    unsigned int size;
    char name[16];
} CdlFILE;

#define CdlNop 0x01
#define CdlSetloc 0x02
#define CdlStandby 0x07
#define CdlStop 0x08
#define CdlPause 0x09
#define CdlSetmode 0x0E
#define CdlSeekL 0x15
#define CdlModeSpeed 0x80
#define CdlComplete 0x02
#define CdlDiskError 0x05
#define CdlStatError 0x01
#define CdlStatStandby 0x02
#define CdlStatShellOpen 0x10

int CdInit(void);
int CdReset(int mode);
int CdIntToPos(int i, CdlLOC *p);
int CdRead(int sectors, void *buf, int mode);
int CdReadSync(int mode, unsigned char *result);
int CdDiskReady(int mode);
int func_8003DE9C(int com, void *param, void *result); /* probably CdControl */
int func_8003DFD4(int com, void *param);               /* probably CdControlF */
int func_8003E100(int com, void *param, void *result); /* probably CdControlB */
int func_8003DE2C(int mode, unsigned char *result);    /* probably CdSync */
int func_8003E244(CdlATV *vol);                        /* probably CdMix */
CdlFILE *func_8003FB8C(CdlFILE *file, const char *name); /* probably CdSearchFile */
int func_8004076C(void);                               /* probably CdGetDiskType */
void func_8003DDA4(int mode);
void func_8003DD84(void);
void func_8003DE6C(int);
void func_8003DE84(int);
int CdRead2(int mode);
void cd_set_volume(int left, int left_to_right, int right, int right_to_left);

/* The movie player (movie.c). */
void movie_init(void);
void movie_load_table(void);
void movie_stop(void);

typedef struct CdRequest {
    unsigned int state;
    int disc; /* from DISKINFO.CNF, -1 if it can't be read */
    CdlLOC loc;
    unsigned int sectors;
    int pad_10[4];
    void *dest;
    void (*callback)(void);
} CdRequest;
extern CdRequest cd_request;

#endif
