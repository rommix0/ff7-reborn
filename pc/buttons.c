/* Button glyphs for the PC's controls (docs/superpowers/specs/2026-11-09-pc-buttons-design.md). The text codes F6-F9
 * draw 16x16 glyphs from WINDOW.BIN's first texture, which the game uploads once to VRAM (960, 256), 4-bit: the face
 * buttons at texel (0x60, 0x40), o x [] /\, the shoulders below them, R1 R2 L1 L2, and Select and Start below those
 * (Config's Customize screen shows all ten). The port draws its own over them, in the texture's colours and the style
 * of the game's (a dark disc, a glint, two-texel strokes), when the texture arrives and when the player switches
 * device; the PlayStation set is the game's own. */
#include <math.h>
#include <string.h>
#include "gpu.h"

#define STRIP_X (960 + 0x60 / 4) /* in halfwords: four texels each */
#define STRIP_Y (256 + 0x40)

enum { GREY = 1, WHITE, RED, BLUE, PINK, GREEN, YELLOW, DARK = 15 };

/* 8x10 letters for the face buttons, 6x8 ones for the shoulders (as the game's R1). */
static const struct {
    char ch;
    const char *rows[10];
} big[] = {
    {'A', {"  ####  ", " ###### ", "###  ###", "##    ##", "##    ##", "########", "########", "##    ##", "##    ##",
           "##    ##"}},
    {'B', {"####### ", "########", "##    ##", "##   ###", "####### ", "####### ", "##   ###", "##    ##", "########",
           "####### "}},
    {'X', {"##    ##", "##    ##", "###  ###", " ###### ", "  ####  ", "  ####  ", " ###### ", "###  ###", "##    ##",
           "##    ##"}},
    {'Y', {"##    ##", "##    ##", "###  ###", " ###### ", "  ####  ", "   ##   ", "   ##   ", "   ##   ", "   ##   ",
           "   ##   "}},
    {'J', {"   #####", "   #####", "      ##", "      ##", "      ##", "      ##", "##    ##", "##    ##", "########",
           " ###### "}},
    {'K', {"##    ##", "##   ###", "##  ### ", "## ###  ", "#####   ", "#####   ", "## ###  ", "##  ### ", "##   ###",
           "##    ##"}},
    {'L', {"##      ", "##      ", "##      ", "##      ", "##      ", "##      ", "##      ", "##      ", "########",
           "########"}},
    {'I', {"  ####  ", "  ####  ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "  ####  ",
           "  ####  "}},
    {'E', {"########", "########", "##      ", "##      ", "######  ", "######  ", "##      ", "##      ", "########",
           "########"}},
    {'Q', {" ###### ", "########", "##    ##", "##    ##", "##    ##", "##    ##", "## ## ##", "##  ### ", "########",
           " ### ###"}},
    {'1', {"   ##   ", "  ###   ", " ####   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", " ###### ",
           " ###### "}},
    {'3', {" ###### ", "########", "##    ##", "      ##", "  ##### ", "  ##### ", "      ##", "##    ##", "########",
           " ###### "}},
    {'C', {" ###### ", "########", "##      ", "##      ", "##      ", "##      ", "##      ", "##      ", "########",
           " ###### "}},
    {'D', {"####### ", "########", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "########",
           "####### "}},
    {'F', {"########", "########", "##      ", "##      ", "######  ", "######  ", "##      ", "##      ", "##      ",
           "##      "}},
    {'G', {" ###### ", "########", "##      ", "##      ", "##  ####", "##  ####", "##    ##", "##    ##", "########",
           " ###### "}},
    {'H', {"##    ##", "##    ##", "##    ##", "##    ##", "########", "########", "##    ##", "##    ##", "##    ##",
           "##    ##"}},
    {'M', {"##    ##", "###  ###", "########", "## ## ##", "## ## ##", "##    ##", "##    ##", "##    ##", "##    ##",
           "##    ##"}},
    {'N', {"##    ##", "###   ##", "####  ##", "## ## ##", "## ## ##", "##  ####", "##   ###", "##    ##", "##    ##",
           "##    ##"}},
    {'O', {" ###### ", "########", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "########",
           " ###### "}},
    {'P', {"####### ", "########", "##    ##", "##    ##", "########", "####### ", "##      ", "##      ", "##      ",
           "##      "}},
    {'R', {"####### ", "########", "##    ##", "##    ##", "####### ", "####### ", "##  ### ", "##   ###", "##    ##",
           "##    ##"}},
    {'S', {" ###### ", "########", "##      ", "###     ", " ###### ", "  ######", "      ##", "      ##", "########",
           " ###### "}},
    {'T', {"########", "########", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ", "   ##   ",
           "   ##   "}},
    {'U', {"##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "########",
           " ###### "}},
    {'V', {"##    ##", "##    ##", "##    ##", "##    ##", "##    ##", "###  ###", " ##  ## ", " ###### ", "  ####  ",
           "   ##   "}},
    {'W', {"##    ##", "##    ##", "##    ##", "##    ##", "## ## ##", "## ## ##", "########", "########", "###  ###",
           "##    ##"}},
    {'Z', {"########", "########", "     ###", "    ### ", "   ###  ", "  ###   ", " ###    ", "###     ", "########",
           "########"}},
    {'0', {" ###### ", "########", "##   ###", "##  ####", "## ## ##", "## ## ##", "####  ##", "###   ##", "########",
           " ###### "}},
    {'2', {" ###### ", "########", "##    ##", "     ###", "    ### ", "   ###  ", "  ###   ", " ###    ", "########",
           "########"}},
    {'4', {"    ### ", "   #### ", "  ## ## ", " ##  ## ", "##   ## ", "########", "########", "     ## ", "     ## ",
           "     ## "}},
    {'5', {"########", "########", "##      ", "##      ", "####### ", "########", "      ##", "      ##", "########",
           "####### "}},
    {'6', {" ###### ", "########", "##      ", "##      ", "####### ", "########", "##    ##", "##    ##", "########",
           " ###### "}},
    {'7', {"########", "########", "     ###", "    ### ", "   ###  ", "   ##   ", "   ##   ", "   ##   ", "   ##   ",
           "   ##   "}},
    {'8', {" ###### ", "########", "##    ##", "##    ##", " ###### ", " ###### ", "##    ##", "##    ##", "########",
           " ###### "}},
    {'9', {" ###### ", "########", "##    ##", "##    ##", "########", " #######", "      ##", "      ##", "########",
           " ###### "}},
    {'\r', {"      ##", "      ##", "      ##", "  #   ##", " ##   ##", "########", "########", " ##     ", "  #     ",
            "        "}}, /* Enter */
    {'\t', {"        ", "      ##", "   #  ##", "   ## ##", "##### ##", "##### ##", "   ## ##", "   #  ##", "      ##",
            "        "}}, /* Tab */
};
static const struct {
    char ch;
    const char *rows[8];
} small[] = {
    {'R', {"##### ", "##  ##", "##  ##", "##### ", "####  ", "## ## ", "##  ##", "##  ##"}},
    {'B', {"##### ", "##  ##", "##  ##", "##### ", "##  ##", "##  ##", "##  ##", "##### "}},
    {'L', {"##    ", "##    ", "##    ", "##    ", "##    ", "##    ", "######", "######"}},
    {'T', {"######", "######", "  ##  ", "  ##  ", "  ##  ", "  ##  ", "  ##  ", "  ##  "}},
};

#define ROWS 48 /* faces, shoulders, Select and Start */

static unsigned short original[ROWS][16]; /* the game's strips, as uploaded */
static int loaded, set;
/* Each pad bit's key cap, as controls.c binds them by default (the window passes those it reads). */
static char labels[16] = {'\t', 0, 0, '\r', 'W', 'D', 'S', 'A', '1', '3', 'Q', 'E', 'I', 'J', 'K', 'L'};

static void stamp(unsigned char cell[ROWS][64], const char *const *rows, int count, int colour, int top, int left)
{
    int r, c;

    for (r = 0; r < count; r++)
        for (c = 0; rows[r][c]; c++)
            if (rows[r][c] == '#')
                cell[top + r][left + c] = colour;
}

/* A round button as the game draws o: a dark disc and its glint, the top row at top. */
static void disc(unsigned char texels[ROWS][64], int top, int at)
{
    int x, y;

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            texels[top + y][at + x] = hypot(x - 7.5, y - 7.5) <= 7.9 ? DARK : 0;
    texels[top + 2][at + 3] = texels[top + 4][at + 2] = GREY;
    texels[top + 3][at + 2] = WHITE;
}

static void face(unsigned char texels[ROWS][64], int at, char letter, int colour)
{
    int i;

    disc(texels, 0, at);
    for (i = 0; big[i].ch != letter; i++)
        ;
    stamp(texels, big[i].rows, 10, colour, 3, at + 4);
}

/* A shoulder button as the game draws R1: a bar with its bevel, pink to red, along the top and left, and two letters
 * in colour 1 (the menu draws this strip with a palette of its own, where only those read as the game's do). */
static void shoulder(unsigned char texels[ROWS][64], int at, const char *name)
{
    static const char top[] = "55544333333333", left[] = "55443333";
    int x, y, n, i;

    for (x = 0; x < 14; x++)
        texels[19][at + 1 + x] = top[x] - '0';
    for (y = 0; y < 8; y++) {
        texels[20 + y][at] = left[y] - '0';
        for (x = 1; x < 16; x++)
            texels[20 + y][at + x] = DARK;
    }
    for (x = 1; x < 15; x++)
        texels[28][at + x] = DARK;
    for (n = 0; n < 2; n++) {
        for (i = 0; small[i].ch != name[n]; i++)
            ;
        stamp(texels, small[i].rows, 8, GREY, 20, at + 2 + 7 * n);
    }
}

/* Xbox's View and Menu buttons over Select and Start: small round buttons, their icons in colour 1. */
static void view_and_menu(unsigned char texels[ROWS][64])
{
    int x, y;

    disc(texels, 32, 0);
    disc(texels, 32, 16);
    for (x = 0; x < 6; x++) /* View: two windows, the front one over the back */
        texels[36][6 + x] = texels[41][6 + x] = texels[39][3 + x] = texels[44][3 + x] = GREY;
    for (y = 0; y < 6; y++) {
        texels[36 + y][6] = texels[36 + y][11] = GREY;
        texels[39 + y][3] = texels[39 + y][8] = GREY;
    }
    for (y = 40; y < 44; y++)
        for (x = 4; x < 8; x++)
            texels[y][x] = DARK;
    for (y = 37; y < 45; y += 3) /* Menu: three lines */
        for (x = 4; x < 12; x++)
            texels[y][16 + x] = GREY;
}

/* A key cap with room round it: a grey edge, two texels deep at the bottom, a dark top, the key on it. */
static void key(unsigned char texels[ROWS][64], int top, int at, char name, int colour)
{
    int x, y, i;

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            texels[top + y][at + x] = y && x && x < 15 ? (y > 1 && y < 13 && x > 1 && x < 14 ? DARK : GREY) : 0;
    texels[top + 1][at + 1] = texels[top + 1][at + 14] = texels[top + 15][at + 1] = texels[top + 15][at + 14] = 0;
    for (i = 0; i < (int)(sizeof big / sizeof big[0]); i++)
        if (name && big[i].ch == name)
            stamp(texels, big[i].rows, 10, colour, top + 2, at + 4);
}

static void draw(void)
{
    unsigned char texels[ROWS][64];
    int x, y;

    if (!loaded)
        return;
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < 64; x++)
            texels[y][x] = original[y][x / 4] >> x % 4 * 4 & 15;
    if (set == BUTTONS_XBOX) {
        /* by function: A is OK (o), B cancel (x) */
        face(texels, 0, 'A', GREEN);
        face(texels, 16, 'B', RED);
        face(texels, 32, 'X', BLUE);
        face(texels, 48, 'Y', YELLOW);
        shoulder(texels, 0, "RB");
        shoulder(texels, 16, "RT");
        shoulder(texels, 32, "LB");
        shoulder(texels, 48, "LT");
        for (y = 32; y < ROWS; y++)
            for (x = 0; x < 32; x++)
                texels[y][x] = 0;
        view_and_menu(texels);
    }
    if (set == BUTTONS_KEYS) { /* the bound keys; the lower strips in colour 1, as the game's shoulders */
        key(texels, 0, 0, labels[13], WHITE);  /* o */
        key(texels, 0, 16, labels[14], WHITE); /* x */
        key(texels, 0, 32, labels[15], WHITE); /* [] */
        key(texels, 0, 48, labels[12], WHITE); /* /\ */
        key(texels, 16, 0, labels[11], GREY);  /* R1 */
        key(texels, 16, 16, labels[9], GREY);  /* R2 */
        key(texels, 16, 32, labels[10], GREY); /* L1 */
        key(texels, 16, 48, labels[8], GREY);  /* L2 */
        key(texels, 32, 0, labels[0], GREY);   /* Select */
        key(texels, 32, 16, labels[3], GREY);  /* Start */
    }
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < 16; x++)
            vram[STRIP_Y + y][STRIP_X + x] = texels[y][4 * x] | texels[y][4 * x + 1] << 4
                                             | texels[y][4 * x + 2] << 8 | texels[y][4 * x + 3] << 12;
}

/* After each upload to VRAM: the window texture arriving keeps the game's strips and draws the set in use; anything
 * else written over them leaves them alone until the texture comes again. */
void buttons_uploaded(int x, int y, int w, int h)
{
    if (x == 960 && y == 256 && w == 64 && h == 256) {
        for (y = 0; y < ROWS; y++)
            memcpy(original[y], &vram[STRIP_Y + y][STRIP_X], sizeof original[y]);
        loaded = 1;
        draw();
        return;
    }
    if (x < STRIP_X + 16 && x + w > STRIP_X && y < STRIP_Y + ROWS && y + h > STRIP_Y)
        loaded = 0;
}

void buttons_show(int which)
{
    if (which == set)
        return;
    set = which;
    draw();
}

void buttons_keys(const char keys[16])
{
    memcpy(labels, keys, sizeof labels);
    if (set == BUTTONS_KEYS)
        draw();
}
