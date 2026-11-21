/* DSCHANGE.X, 0x800A0000-0x800A1580 (its data follows to 0x800A1774): main loads it in the field's place for the screen
 * that asks for another disc and for the game over screen. Both show a picture in 640x480 interlaced, uploading only
 * the lines of the field on screen each frame, and fade it with a subtractive tile. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "menu.h"
#include "prims.h"
#include "cd.h"
#include "sound.h"

/* Sector and size pairs: the prompts for discs 1-3, then nine backgrounds. */
extern unsigned int dsc_pictures[24];
extern int dsc_buffer;
extern DRAWENV dsc_draw[2];
extern DISPENV dsc_disp[2];
extern DISPENV *dsc_cur_disp;
extern DRAWENV *dsc_cur_draw;
/* Both pads, as func_8001C808 reads them in one word: pad 1, then pad 2. */
extern unsigned int dsc_released1, dsc_released2, dsc_held1, dsc_held2, dsc_pressed1, dsc_pressed2;
extern unsigned int dsc_ot[2];
extern TILE dsc_tiles[2];
extern DR_MODE dsc_modes[2];

int cd_read_wait(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_read_lzs_wait(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
void cd_stop(void);
int cd_check_disc(void);
int get_disc_number(void);
void SetDispMask(int mask);
int func_800484A8(void); /* BreakDraw */
int IsIdleGPU(int max);
void func_80043938(int mode); /* ResetGraph */
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
int func_80044908(void); /* GetODE: the field being drawn */
void func_80044000(RECT *r, void *p); /* LoadImage */
void func_80044244(unsigned int *ot, int n);
void SetTile(TILE *p);
void func_80044A68(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */

/* The soft reset: L1, L2, R1, R2, START and SELECT held. */
#define DSC_RESET 0x90F

/* The frame both screens run: show what was drawn, swap buffers, read the pads. */
#define DSC_FLIP()                                                                                                     \
    {                                                                                                                  \
        unsigned int pad;                                                                                              \
                                                                                                                       \
        DrawSync(0);                                                                                                   \
        VSync(0);                                                                                                      \
        dsc_buffer = !dsc_buffer;                                                                                      \
        func_800444AC(&dsc_disp[dsc_buffer]);                                                                          \
        PutDrawEnv(&dsc_draw[dsc_buffer]);                                                                             \
        dsc_cur_disp = &dsc_disp[dsc_buffer];                                                                          \
        dsc_cur_draw = &dsc_draw[dsc_buffer];                                                                          \
        pad = func_8001C808();                                                                                         \
        dsc_pressed1 = dsc_released1 & pad;                                                                            \
        dsc_pressed2 = dsc_released2 & pad >> 16;                                                                      \
        dsc_held1 = pad;                                                                                               \
        dsc_held2 = pad >> 16;                                                                                         \
        dsc_released1 = ~pad;                                                                                          \
        dsc_released2 = ~(pad >> 16);                                                                                  \
    }

/* Draws a subtractive tile of brightness b over the screen of buffer i. */
#define DSC_FADE(i, b, _x, _y, _w, _h)                                                                                 \
    {                                                                                                                  \
        unsigned int *ot;                                                                                              \
        TILE *tile;                                                                                                    \
                                                                                                                       \
        ot = &dsc_ot[i];                                                                                               \
        func_80044244(ot, 1);                                                                                          \
        tile = &dsc_tiles[i];                                                                                          \
        SetTile(tile);                                                                                                 \
        func_80046848(tile, 1);                                                                                        \
        tile->r0 = (b);                                                                                                \
        tile->g0 = (b);                                                                                                \
        tile->b0 = (b);                                                                                                \
        tile->x0 = (_x);                                                                                               \
        tile->y0 = (_y);                                                                                               \
        tile->w = (_w);                                                                                                \
        tile->h = (_h);                                                                                                \
        AddPrim(ot, tile);                                                                                             \
        func_80044A68(&dsc_modes[i], 0, 0, GetTPage(2, 2, 0, 0), 0);                                                   \
        AddPrim(ot, &dsc_modes[i]);                                                                                    \
        func_8004433C(ot);                                                                                             \
    }

/* Sets up the GPU for both screens: 640x480 interlaced, both buffers in the same place. An inline function in the game
 * too: each call loads its env's address afresh, and 480, 0 and BreakDraw's -1 sit in saved registers from before the
 * reset loop. */
static inline void dsc_init(void)
{
    int busy;
    int h = 480, y = 0, none = -1;

    while ((busy = func_800484A8()) == none)
        VSync(0);
    if (busy)
        while (IsIdleGPU(1))
            ;
    func_80043938(1);
    func_80043814(dsc_draw, 0, 0, 640, h);
    SetDefDispEnv(dsc_disp, 0, y, 640, h);
    func_80043814(&dsc_draw[1], 0, y, 640, h);
    SetDefDispEnv(&dsc_disp[1], 0, 0, 640, h);
    dsc_disp[0].isrgb24 = dsc_disp[1].isrgb24 = 0;
    dsc_disp[0].isinter = dsc_disp[1].isinter = 1;
    dsc_draw[0].isbg = dsc_draw[1].isbg = 0;
    dsc_draw[0].dfe = dsc_draw[1].dfe = 0;
    dsc_draw[0].dtd = dsc_draw[1].dtd = 0;
    dsc_draw[0].tpage = dsc_draw[1].tpage = 0;
    dsc_draw[0].r0 = 0;
    dsc_draw[0].g0 = 0;
    dsc_draw[0].b0 = 0;
    dsc_draw[1].r0 = 0;
    dsc_draw[1].g0 = 0;
    dsc_draw[1].b0 = 0;
    dsc_buffer = 0;
}

/* Asks for disc `disc` until it's in: its prompt over one of nine pictures, faded in, then held until a button or the
 * lid opening and closing, and faded out before the disc is checked again. Returns 1 for a soft reset, 0 once the
 * disc is in. */
#ifdef NON_MATCHING
/* Same length: the first index's - 1 and the read's buffer address come in the other order, and the % 9 quotient
 * goes through another register. */
int dsc_disc_change(int disc)
{
    RECT rect;
    unsigned char result[8];
    RECT unused; /* unused, but the game's frame has it */
    short b;
    int reset;
    int odd;
    int i;
    int n;
    unsigned int *lba = &dsc_pictures[0];
    unsigned int *size = &dsc_pictures[1];
    unsigned char *src;

    n = disc - 1;
    cd_read_lzs_wait(lba[n * 2], size[n * 2], (void *)0x800B0000, 0);
    n = VSync(-1) % 9 * 2 + 6;
    cd_read_lzs(lba[n], size[n], (void *)0x800C8000, 0);
    SetDispMask(0);
    dsc_init();
    setRECT(&rect, 0, 0, 640, 480);
    ClearImage(&rect, 0, 0, 0);
    DSC_FLIP();
    while (cd_step())
        ;
    DSC_FLIP();
    SetDispMask(1);
    for (;;) {
        b = 255;
        do {
            DSC_FLIP();
            i = dsc_buffer;
            odd = func_80044908() ^ 1;
            n = 0x800B0000 + odd * 800; /* through n, which the game's registers show */
            src = (unsigned char *)n;
            setRECT(&rect, 120, 340, 400, 1);
            rect.y = 340 + odd;
            while (rect.y < 460) {
                func_80044000(&rect, src);
                rect.y += 2;
                src += 1600;
            }
            src = (unsigned char *)0x800C8000 + odd * 800;
            setRECT(&rect, 120, 32, 400, 1);
            rect.y = 32 + odd;
            while (rect.y < 332) {
                func_80044000(&rect, src);
                rect.y += 2;
                src += 1600;
            }
            DSC_FADE(i, b, 120, 32, 400, 448);
        } while ((b -= 4) >= 0);
        cd_stop();
        reset = 0;
        do {
            DSC_FLIP();
            if (dsc_held1 == DSC_RESET)
                goto soft_reset;
            if (dsc_pressed1 & 0xF0)
                goto fade_out;
            func_8003E100(CdlNop, 0, result);
        } while (!(result[0] & CdlStatShellOpen));
        do {
            DSC_FLIP();
            if (dsc_held1 == DSC_RESET)
                goto soft_reset;
            if (dsc_pressed1 & 0xF0)
                break;
            func_8003E100(CdlNop, 0, result);
        } while (result[0] & CdlStatShellOpen);
fade_out:
        b = 64;
        do {
            DSC_FLIP();
            i = dsc_buffer;
            DSC_FADE(i, 8, 0, 0, 640, 480);
        } while (--b);
        if (cd_check_disc())
            continue;
        if (reset == 1)
            return 1;
        if (get_disc_number() == disc)
            return 0;
        continue;
soft_reset:
        reset = 1;
        goto fade_out;
    }
}
#else
INCLUDE_ASM("asm/overlay/dschange/nonmatchings/dschange", dsc_disc_change);
#endif

/* The game over screen: its music and picture, faded in, held until a button, then faded out with the music. */
void dsc_game_over(void)
{
    RECT rect;
    RECT unused; /* unused, but the game's frame has it */
    short b;
    int odd;
    int i;
    unsigned char *src;

    SetDispMask(0);
    dsc_init();
    cd_read_wait(0x1F480, 0x800, (void *)0x800B0000, 0);
    snd_request.id = 0x10;
    snd_request.arg[0] = 0x800B0000;
    snd_submit_request();
    cd_read_lzs_wait(0x1F400, 0x40000, (void *)0x800B0000, 0);
    setRECT(&rect, 0, 0, 640, 480);
    ClearImage(&rect, 0, 0, 0);
    DSC_FLIP();
    while (cd_step())
        ;
    SetDispMask(1);
    b = 255;
    do {
        DSC_FLIP();
        i = dsc_buffer;
        odd = func_80044908() ^ 1;
        src = (unsigned char *)0x800B0000 + odd * 1152;
        setRECT(&rect, 32, 32, 576, 1);
        rect.y = 32 + odd;
        while (rect.y < 448) {
            func_80044000(&rect, src);
            rect.y += 2;
            src += 2304;
        }
        DSC_FADE(i, b, 32, 32, 576, 416);
        if (dsc_pressed1 & 0x9F0)
            goto fade_out;
    } while ((b -= 4) >= 0);
    do {
        DSC_FLIP();
    } while (!(dsc_pressed1 & 0x9F0));
fade_out:
    snd_request.id = 0xC1;
    snd_request.arg[0] = 60;
    snd_request.arg[1] = 0;
    snd_submit_request();
    b = 64;
    do {
        DSC_FLIP();
        i = dsc_buffer;
        DSC_FADE(i, 8, 0, 0, 640, 480);
    } while (--b);
}
