/* The GTE against psx-spx's formulas. */
#include <assert.h>
#include <stdio.h>

void gte_cmd(unsigned int op);
unsigned int gte_read(int reg);
void gte_write(int reg, unsigned int v);
unsigned int gte_read_ctrl(int reg);
void gte_write_ctrl(int reg, unsigned int v);

#define RTPS 0x0180001u
#define NCLIP 0x1400006u
#define AVSZ3 0x158002Du
#define MVMVA_RT_V0_NONE 0x0406012u /* sf=0; add 0x80000 for sf=1 */
#define FLAG 31

static void identity_rotation(void)
{
    gte_write_ctrl(0, 0x1000);       /* RT11 = 1.0, RT12 = 0 */
    gte_write_ctrl(1, 0);            /* RT13, RT21 */
    gte_write_ctrl(2, 0x1000);       /* RT22 = 1.0, RT23 */
    gte_write_ctrl(3, 0);            /* RT31, RT32 */
    gte_write_ctrl(4, 0x1000);       /* RT33 */
}

static void rtps_projects(void)
{
    identity_rotation();
    gte_write_ctrl(5, 0);            /* TR = (0, 0, 1000) */
    gte_write_ctrl(6, 0);
    gte_write_ctrl(7, 1000);
    gte_write_ctrl(24, 0);           /* OFX, OFY */
    gte_write_ctrl(25, 0);
    gte_write_ctrl(26, 1000);        /* H */
    gte_write(0, 50 << 16 | 100);    /* V0 = (100, 50, 0) */
    gte_write(1, 0);
    gte_cmd(RTPS);
    assert(gte_read(14) == (50u << 16 | 100));
    assert(gte_read(19) == 1000 && !(gte_read_ctrl(FLAG) & 1 << 17));

    gte_write_ctrl(7, 400);          /* SZ3 400 < H/2: the divide overflows */
    gte_cmd(RTPS);
    assert(gte_read_ctrl(FLAG) & 1 << 17 && gte_read_ctrl(FLAG) & 1u << 31);
}

static void nclip_sign(void)
{
    int clockwise, anticlockwise;

    gte_write(12, 0);                /* (0,0) (10,0) (0,10) */
    gte_write(13, 10);
    gte_write(14, 10 << 16);
    gte_cmd(NCLIP);
    clockwise = gte_read(24);
    gte_write(13, 10 << 16);
    gte_write(14, 10);
    gte_cmd(NCLIP);
    anticlockwise = gte_read(24);
    assert(clockwise == -anticlockwise && clockwise != 0);
}

static void mvmva_shift(void)
{
    identity_rotation();
    gte_write(0, 0x1000);            /* V0 = (1.0, 0, 0) */
    gte_write(1, 0);
    gte_cmd(MVMVA_RT_V0_NONE | 0x80000);
    assert(gte_read(25) == 0x1000 && gte_read(9) == 0x1000);
    gte_cmd(MVMVA_RT_V0_NONE);
    assert(gte_read(25) == 0x1000000 && gte_read(9) == 0x7FFF && gte_read_ctrl(FLAG) & 1 << 24);
}

static void avsz3_average(void)
{
    gte_write(17, 100);
    gte_write(18, 200);
    gte_write(19, 300);
    gte_write_ctrl(29, 0x555);       /* ZSF3 = 1/3 in 4.12 */
    gte_cmd(AVSZ3);
    assert(gte_read(7) == 199);
}

static void registers(void)
{
    gte_write(30, 0x0000FFFF);
    assert(gte_read(31) == 16);
    gte_write(30, 0xFFFF0000);       /* negative: leading ones */
    assert(gte_read(31) == 16);
    gte_write(12, 1);                /* SXYP pushes the FIFO */
    gte_write(13, 2);
    gte_write(14, 3);
    gte_write(15, 4);
    assert(gte_read(12) == 2 && gte_read(13) == 3 && gte_read(14) == 4 && gte_read(15) == 4);
    gte_write(28, 31 | 16 << 5 | 1 << 10); /* IRGB */
    assert(gte_read(9) == 31 * 0x80 && gte_read(10) == 16 * 0x80 && gte_read(11) == 0x80);
    assert(gte_read(29) == (31 | 16 << 5 | 1 << 10));
}

/* ir_read_before_written: libgte's OuterProduct0 as the field runs it on a walkmesh triangle's edges: the first edge in the rotation
 * matrix's diagonal, the second in IR, then op 0 (no shift); MAC is the cross product */
static void ir_read_before_written(void)
{
    gte_write_ctrl(0, -259);
    gte_write_ctrl(2, -51);
    gte_write_ctrl(4, 0);
    gte_write(9, 20);
    gte_write(10, 183);
    gte_write(11, 0);
    gte_cmd(0x170000C);
    assert(gte_read(25) == 0 && gte_read(26) == 0 && (int)gte_read(27) == -259 * 183 - -51 * 20);
    /* and MVMVA on IR reads all of IR before it writes any: a matrix that swaps x and y */
    gte_write_ctrl(0, 0x1000 << 16);
    gte_write_ctrl(1, 0x1000 << 16);
    gte_write_ctrl(2, 0);
    gte_write_ctrl(3, 0);
    gte_write_ctrl(4, 0x1000);
    gte_write(9, 100);
    gte_write(10, 200);
    gte_write(11, 300);
    gte_cmd(0x9E012); /* sf, RT x IR, no translation */
    assert(gte_read(9) == 200 && gte_read(10) == 100 && gte_read(11) == 300);
}

int main(void)
{
    ir_read_before_written();
    rtps_projects();
    nclip_sign();
    mvmva_shift();
    avsz3_average();
    registers();
    puts("ok");
    return 0;
}
