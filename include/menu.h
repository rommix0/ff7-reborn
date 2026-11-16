#ifndef MENU_H
#define MENU_H

/* The exe's menu code as the menu modules (the .MNU files in MENU) and the exe's own menu files use it. */

typedef struct PadState {
    unsigned short held, prev, pressed, repeat;
} PadState;
extern PadState field_pad1;
/* field_pad1.pressed and .repeat, which the modules read as variables of their own: a struct field there costs GCC a
 * register. */
extern unsigned short D_80062D7C;
extern unsigned short D_80062D7E;

extern int results_buf; /* the draw buffer, 0 or 1 */
extern int D_80062F90;
extern unsigned char D_800756F8[];
extern unsigned char D_80049208[12]; /* the window's four corner colours */

unsigned int func_8001C808(void); /* the pad's buttons as read */
void func_8001CB48(void);
void func_8001CF3C(int x, int y, int w, int h, int u, int v, int tw, int th, int pal, int semi); /* takes shorts */
void func_8001D180(int x, int y, int w, int h, int u, int v, int tw, int th, int pal, int semi); /* takes shorts */
void func_8001DE0C(RECT *r, int x, int y, int w, int h);
void func_8001DE70(void);                  /* saves the window colours */
void func_8001DEB0(void);                  /* puts them back */
void func_8001DEF0(unsigned char *colors); /* sets them */
void func_8001DF24(RECT *r, unsigned char red, unsigned char green, unsigned char blue); /* a plain rectangle */
void func_8001E040(RECT *r);
void func_8001EB2C(int x, int y); /* takes shorts, but its callers pass ints unextended */
unsigned char func_8001F6B4(void); /* the caption's state */
void func_8001F6C0(unsigned char *text, int color); /* opens the caption */
void func_80020B68(int x, int y, int slot);
void func_80020058(int slot);
void func_800206E4(int x, int y, int rec); /* a character's name, level, HP and MP */
void func_80021044(DRAWENV *draw, DISPENV *disp);
void func_8002120C(int module);
unsigned char *func_80021280(int item); /* an item's name, the inventory's numbering */
int func_80023050(void);
void func_8002305C(int state, int command);
void func_800230C4(int buf);
int func_8002542C(int materia); /* adds a materia to the stock */
short func_8002603C(int materia); /* its icon's palette */
void func_80025B8C(void *p);
void func_80025BD0(void *p);
void func_80025C14(void *p);
void func_80025C54(void *p);
void func_80025C94(void *p);
void func_80025CD4(void *p);
void func_80025DF8(void);
void func_800260DC(void);
void func_80026448(MenuCursor *c, int col, int row, int cols, int rows, int pad0, int top, int pad4, int total,
                   int padE, int scroll_step, int wrap_x, int wrap_y, unsigned short scrolling);
void func_800264A8(MenuCursor *cursor);
void func_800269C0(void *buf); /* sets prim_next */
void func_800269D0(void);      /* saves prim_next */
void func_800269E8(void);      /* puts it back */
void func_80026A00(unsigned int *ot);
void func_80026A34(int dfe, int dtd, int tpage, RECT *tw);
int func_80026B70(unsigned char *text); /* text's width in pixels */
int func_80026C5C(int x, int y, int color, unsigned short ch); /* draws a character */
void func_80026F44(int x, int y, unsigned char *text, int color);
void func_8002708C(int x, int y, int glyph, int color);
void func_80027B84(RECT *r); /* a sunken panel */
void func_80028030(RECT *r); /* a raised button */
void func_800285AC(int x, int y, int value, int h, int r, int g, int b);
void func_80028CA0(short x, short y, int u, int v, int w, int h, int pal, int semi);
void func_80028E00(int x, int y, int n, int digits, int color);
void func_80029114(int x, int y, int n, int digits, int color);
void func_8004418C(unsigned int *ot, int n);
void func_8004433C(unsigned int *ot);
void func_800444AC(DISPENV *env);
void func_80046848(void *p, int semi); /* sets a primitive's semi-transparency */
void member_update(unsigned char slot);
void PutDrawEnv(DRAWENV *env);

/* battle.c's and the text's, which the menus use too */
int limit_id(unsigned int character, int slot);
unsigned char *kind_text(int kind, int index, int names);

#endif
