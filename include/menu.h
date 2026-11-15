#ifndef MENU_H
#define MENU_H

/* The exe's menu code as the menu modules (MENU/*.MNU) and the exe's own menu files use it. */

typedef struct PadState {
    unsigned short held, prev, pressed, repeat;
} PadState;
extern PadState field_pad1;
/* field_pad1.repeat, which the modules read as a variable of its own: a struct field there costs GCC a register. */
extern unsigned short D_80062D7E;

extern int results_buf; /* the draw buffer, 0 or 1 */
extern int D_80062F90;
extern unsigned char D_800756F8[];

void func_8001D180(int x, int y, int w, int h, int u, int v, int tw, int th, int pal, int semi); /* takes shorts */
void func_8001DE0C(RECT *r, int x, int y, int w, int h);
void func_8001E040(RECT *r);
void func_8001EB2C(int x, int y); /* takes shorts, but its callers pass ints unextended */
void func_8002120C(int module);
void func_8002305C(int state, int command);
void func_800230C4(int buf);
void func_80025B8C(void *p);
void func_80025BD0(void *p);
void func_80025C14(void *p);
void func_80025C54(void *p);
void func_80025C94(void *p);
void func_80025DF8(void);
void func_800260DC(void);
void func_80026448(MenuCursor *c, int col, int row, int cols, int rows, int pad0, int top, int pad4, int total,
                   int padE, int scroll_step, int wrap_x, int wrap_y, unsigned short scrolling);
void func_800264A8(MenuCursor *cursor);
void func_80026A34(int dfe, int dtd, int tpage, RECT *tw);
void func_80026F44(int x, int y, unsigned char *text, int color);
void func_80028E00(int x, int y, int n, int digits, int color);

#endif
