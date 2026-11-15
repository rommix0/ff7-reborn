/* CHNGMENU.MNU, 0x801D0000-0x801D01C4 (its data follows to 0x801D0218): module 6, the Order command's, which the menu
 * never loads (menu.c runs Order itself on the party screen). A title, two windows and a list cursor, left with ✕; the
 * title's bytes aren't text in the US font. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "menu.h"

extern int chng_active; /* the cursor in use; -1 until chng_init */
extern RECT chng_windows[2];
extern unsigned char chng_title[];
extern MenuCursor chng_cursors[2];

void chng_init(void)
{
    RECT unused; /* unused, but the game's frame has it */

    func_80026448(&chng_cursors[0], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&chng_cursors[1], 0, 0, 1, 3, 0, 0, 1, 9, 0, 0, 0, 0, 0);
    chng_active = 0;
}

void chng_frame(void)
{
    RECT unused[2]; /* unused, but the game's frame has them */
    int i;

    func_800230C4(results_buf);
    if (!chng_active)
        func_8001EB2C(0, chng_windows[1].y + chng_cursors[0].row * 64 + 32);
    func_80026F44(16, 11, chng_title, 7);
    for (i = 0; i < 2; i++)
        func_8001E040(&chng_windows[i]);
    func_800264A8(&chng_cursors[chng_active]);
    if (field_pad1.repeat & 0x40) {
        func_8002305C(5, 0);
        func_8002120C(0);
    }
}

void chng_801D01BC(void)
{
}
