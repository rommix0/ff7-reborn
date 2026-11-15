/* PATYMENU.MNU, 0x801D0198-0x801D065C (read-only data before it, data after it to 0x801D74CC): module 11, a debug
 * party editor that nothing in the exe opens. It shows the three party places with each character's name and
 * portrait; ↑/↓ pick a place, ←/→ step its character id (0xFF for none), ○ switches the character's row and ✕ leaves.
 * Its read-only data is the developers' paths of the portrait TIMs, which the code doesn't use. GCC 2.7.2,
 * ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "menu.h"

extern int paty_place;
extern unsigned char paty_saved_cluts[];
extern unsigned char paty_saved_faces[];
extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */

/* The portraits take VRAM the menu uses for other things, so what was there is kept until paty_exit. */
void paty_init(void)
{
    RECT unused; /* unused, but the game's frame has it */

    func_80025B8C(paty_saved_faces);
    DrawSync(0);
    func_80025C14(paty_saved_cluts);
    DrawSync(0);
    func_80025DF8();
    DrawSync(0);
}

void paty_exit(void)
{
    func_80025C94(D_800756F8);
    DrawSync(0);
    func_80025BD0(paty_saved_faces);
    DrawSync(0);
    func_80025C54(paty_saved_cluts);
    DrawSync(0);
}

void paty_frame(void)
{
    RECT win;
    RECT tw;
    int i;

    func_800230C4(results_buf);
    func_8001EB2C(16, paty_place * 56 + 52);
    for (i = 0; i < 3; i++) {
        if ((signed char)D_8009CBDC[i] != -1 && D_8009CBDC[i] < 9)
            func_80026F44(160, i * 56 + 56, D_8009C738[D_8009CBDC[i]].name, 7);
        else
            func_80028E00(160, i * 56 + 56, D_8009CBDC[i], 5, 7);
        /* the portrait, further right in the back row */
        if ((signed char)D_8009CBDC[i] != -1 && D_8009CBDC[i] < 9)
            func_8001D180(((D_8009C738[D_8009CBDC[i]].row ^ 1) & 1) * 24 + 80, i * 56 + 36, 48, 48,
                          D_8009CBDC[i] < 5 ? 0 : 48, D_8009CBDC[i] % 5 * 48, 48, 48, D_8009CBDC[i], 0);
        setRECT(&tw, 0, 0, 256, 256);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001DE0C(&win, 0, i * 56 + 32, 0x16C, 56);
        func_8001E040(&win);
    }
    if (D_80062D7E & 0x4000)
        paty_place = (paty_place + 1) % 3;
    if (D_80062D7E & 0x1000) {
        paty_place--;
        if (paty_place < 0)
            paty_place = 2;
    }
    if (D_80062D7E & 0x2000) {
        D_8009CBDC[paty_place]++;
        if (D_8009CBDC[paty_place] == 11)
            D_8009CBDC[paty_place] = 0xFF;
    }
    if (D_80062D7E & 0x8000) {
        D_8009CBDC[paty_place]--;
        if ((signed char)D_8009CBDC[paty_place] == -2)
            D_8009CBDC[paty_place] = 10;
    }
    if (D_80062D7E & 0x20)
        D_8009C738[D_8009CBDC[paty_place]].row ^= 1;
    if (D_80062D7E & 0x40) {
        func_8002305C(5, 0);
        func_8002120C(0);
        func_800260DC();
        D_80062F90 = 1;
        paty_exit();
    }
}
