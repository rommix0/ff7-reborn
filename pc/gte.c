/* The GTE (COP2) in C, from psx-spx's GTE pages: its registers, every command, the saturation flags, and the
 * hardware's Newton-Raphson divide (whose reciprocal table is built from psx-spx's formula). */

static short v[3][3], ir[4], rt[3][3], llm[3][3], lcm[3][3], dqa, zsf3, zsf4;
static int mac[4], tr[3], bk[3], fc[3], ofx, ofy, dqb, lzcs;
static unsigned int rgbc, otz, sxy[3], sz[4], rgb[3], res1, h, flag;
static int sf, lm; /* the current command's shift and lower-limit bits */

#define FLAG_ERROR_BITS 0x7F87E000u /* bits 30-23 and 18-13 feed bit 31 */

static int sext16(unsigned int x)
{
    return (short)x;
}

static unsigned int pack(short lo, short hi)
{
    return (unsigned short)lo | (unsigned int)(unsigned short)hi << 16;
}

/* MACn takes the sum; FLAG notes a result past 44 bits (MAC1-3) or 32 bits (MAC0). */
static long long check_mac(int i, long long x)
{
    if (i == 0) {
        if (x > 0x7FFFFFFFLL)
            flag |= 1 << 16;
        else if (x < -0x80000000LL)
            flag |= 1 << 15;
    } else if (x > 0x7FFFFFFFFFFLL)
        flag |= 1u << (31 - i);
    else if (x < -0x80000000000LL)
        flag |= 1u << (28 - i);
    return x;
}

static int saturate(int x, int lo, int hi, unsigned int bit)
{
    if (x < lo) {
        flag |= bit;
        return lo;
    }
    if (x > hi) {
        flag |= bit;
        return hi;
    }
    return x;
}

static void set_ir(int i, int x, int lower_limit)
{
    ir[i] = saturate(x, lower_limit ? 0 : -0x8000, 0x7FFF, 1u << (25 - i));
}

static void set_mac_ir(int i, long long x)
{
    mac[i] = (int)(check_mac(i, x) >> (sf * 12));
    set_ir(i, mac[i], lm);
}

static void push_color(void)
{
    rgb[0] = rgb[1];
    rgb[1] = rgb[2];
    rgb[2] = saturate(mac[1] >> 4, 0, 255, 1 << 21) | saturate(mac[2] >> 4, 0, 255, 1 << 20) << 8
        | saturate(mac[3] >> 4, 0, 255, 1 << 19) << 16 | (rgbc & 0xFF000000);
}

static int unr_table(int i)
{
    int u = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;

    return u > 0 ? u : 0;
}

/* H / SZ3 as 16.16, the way the hardware approximates it. */
static unsigned int divide(unsigned int hv, unsigned int z)
{
    unsigned long long n, d, u;
    int shift = 0;

    if (hv >= z * 2) {
        flag |= 1 << 17;
        return 0x1FFFF;
    }
    while (shift < 16 && !(z << shift & 0x8000))
        shift++;
    n = (unsigned long long)hv << shift;
    d = z << shift;
    u = unr_table((int)((d - 0x7FC0) >> 7)) + 0x101;
    d = (0x2000080 - d * u) >> 8;
    d = (0x0000080 + d * u) >> 8;
    n = (n * d + 0x8000) >> 16;
    return n > 0x1FFFF ? 0x1FFFF : (unsigned int)n;
}

static void rtps(int i, int last)
{
    long long m3;
    int n, k, ir3_flag_check;

    for (k = 0; k < 2; k++)
        set_mac_ir(k + 1, (long long)tr[k] * 0x1000 + rt[k][0] * v[i][0] + rt[k][1] * v[i][1] + rt[k][2] * v[i][2]);
    m3 = check_mac(3, (long long)tr[2] * 0x1000 + rt[2][0] * v[i][0] + rt[2][1] * v[i][1] + rt[2][2] * v[i][2]);
    mac[3] = (int)(m3 >> (sf * 12));
    ir3_flag_check = (int)(m3 >> 12); /* IR3's flag is always checked as if lm were 0, on MAC3 SAR 12 */
    if (ir3_flag_check < -0x8000 || ir3_flag_check > 0x7FFF)
        flag |= 1 << 22;
    ir[3] = mac[3] < (lm ? 0 : -0x8000) ? (lm ? 0 : -0x8000) : mac[3] > 0x7FFF ? 0x7FFF : mac[3];
    sz[0] = sz[1];
    sz[1] = sz[2];
    sz[2] = sz[3];
    sz[3] = saturate((int)(m3 >> 12), 0, 0xFFFF, 1 << 18);
    n = divide(h, sz[3]);
    sxy[0] = sxy[1];
    sxy[1] = sxy[2];
    sxy[2] = pack(saturate((int)(check_mac(0, (long long)n * ir[1] + ofx) >> 16), -0x400, 0x3FF, 1 << 14),
                  saturate((int)(check_mac(0, (long long)n * ir[2] + ofy) >> 16), -0x400, 0x3FF, 1 << 13));
    if (last) {
        mac[0] = (int)check_mac(0, (long long)n * dqa + dqb);
        ir[0] = saturate(mac[0] >> 12, 0, 0x1000, 1 << 12);
    }
}

/* Matrix times vector plus translation (MVMVA and the light/colour steps). */
static void multiply(short m[3][3], const short *in, const int *add, int far_color_bug)
{
    short vec[3] = {in[0], in[1], in[2]}; /* in may be IR, which the results overwrite */
    long long sum;
    int k;

    for (k = 0; k < 3; k++) {
        sum = (long long)(add ? add[k] : 0) * 0x1000 + m[k][0] * vec[0];
        if (far_color_bug) { /* FC as the translation: only the last two products survive */
            check_mac(k + 1, sum);
            sum = 0;
        }
        set_mac_ir(k + 1, sum + m[k][1] * vec[1] + m[k][2] * vec[2]);
    }
}

/* MAC = MAC + (FC - MAC) * IR0, on the unshifted values; then the shift. */
static void depth_cue(long long *m)
{
    int k;

    for (k = 0; k < 3; k++) {
        set_ir(k + 1, (int)(check_mac(k + 1, ((long long)fc[k] << 12) - m[k]) >> (sf * 12)), 0);
        m[k] = check_mac(k + 1, (long long)ir[k + 1] * ir[0] + m[k]);
    }
}

static void finish_color(long long *m)
{
    int k;

    for (k = 0; k < 3; k++)
        set_mac_ir(k + 1, m[k]);
    push_color();
}

static void light(int i, int color, int cue)
{
    long long m[3];
    int k;

    multiply(llm, v[i], 0, 0);
    multiply(lcm, ir + 1, bk, 0);
    if (!color) {
        push_color();
        return;
    }
    for (k = 0; k < 3; k++)
        m[k] = ((long long)(rgbc >> (8 * k) & 0xFF) * ir[k + 1]) << 4;
    if (cue)
        depth_cue(m);
    finish_color(m);
}

void gte_cmd(unsigned int op)
{
    long long m[3];
    short garbage[3][3], *vec;
    int k, i, mx = op >> 17 & 3, cv = op >> 13 & 3;

    sf = op >> 19 & 1;
    lm = op >> 10 & 1;
    flag = 0;
    switch (op & 0x3F) {
    case 0x01:
        rtps(0, 1);
        break;
    case 0x30:
        for (i = 0; i < 3; i++)
            rtps(i, i == 2);
        break;
    case 0x06: {
        int x0 = sext16(sxy[0]), y0 = sext16(sxy[0] >> 16), x1 = sext16(sxy[1]), y1 = sext16(sxy[1] >> 16);
        int x2 = sext16(sxy[2]), y2 = sext16(sxy[2] >> 16);

        mac[0] = (int)check_mac(0, (long long)x0 * y1 + (long long)x1 * y2 + (long long)x2 * y0
                                       - (long long)x0 * y2 - (long long)x1 * y0 - (long long)x2 * y1);
        break;
    }
    case 0x0C: { /* OP: IR x the rotation matrix's diagonal, all from IR as it was */
        short a[4] = {0, ir[1], ir[2], ir[3]};

        set_mac_ir(1, (long long)a[3] * rt[1][1] - (long long)a[2] * rt[2][2]);
        set_mac_ir(2, (long long)a[1] * rt[2][2] - (long long)a[3] * rt[0][0]);
        set_mac_ir(3, (long long)a[2] * rt[0][0] - (long long)a[1] * rt[1][1]);
        break;
    }
    case 0x10: /* DPCS */
    case 0x2A: /* DPCT: three times, from the colour FIFO's bottom */
        for (i = 0; i < ((op & 0x3F) == 0x2A ? 3 : 1); i++) {
            unsigned int src = (op & 0x3F) == 0x2A ? rgb[0] : rgbc;

            for (k = 0; k < 3; k++)
                m[k] = (long long)(src >> (8 * k) & 0xFF) << 16;
            depth_cue(m);
            finish_color(m);
        }
        break;
    case 0x11: /* INTPL */
        for (k = 0; k < 3; k++)
            m[k] = (long long)ir[k + 1] << 12;
        depth_cue(m);
        finish_color(m);
        break;
    case 0x12: /* MVMVA */
        if (mx == 3) { /* the "garbage" matrix psx-spx describes */
            garbage[0][0] = -(short)((rgbc & 0xFF) << 4);
            garbage[0][1] = (short)((rgbc & 0xFF) << 4);
            garbage[0][2] = ir[0];
            garbage[1][0] = garbage[1][1] = garbage[1][2] = rt[0][2];
            garbage[2][0] = garbage[2][1] = garbage[2][2] = rt[1][1];
        }
        vec = (op >> 15 & 3) == 3 ? ir + 1 : v[op >> 15 & 3];
        multiply(mx == 0 ? rt : mx == 1 ? llm : mx == 2 ? lcm : garbage, vec,
                 cv == 0 ? tr : cv == 1 ? bk : cv == 2 ? fc : 0, cv == 2);
        break;
    case 0x13: /* NCDS */
        light(0, 1, 1);
        break;
    case 0x16: /* NCDT */
        for (i = 0; i < 3; i++)
            light(i, 1, 1);
        break;
    case 0x1B: /* NCCS */
        light(0, 1, 0);
        break;
    case 0x3F: /* NCCT */
        for (i = 0; i < 3; i++)
            light(i, 1, 0);
        break;
    case 0x1E: /* NCS */
        light(0, 0, 0);
        break;
    case 0x20: /* NCT */
        for (i = 0; i < 3; i++)
            light(i, 0, 0);
        break;
    case 0x14: /* CDP */
    case 0x1C: /* CC */
        multiply(lcm, ir + 1, bk, 0);
        for (k = 0; k < 3; k++)
            m[k] = ((long long)(rgbc >> (8 * k) & 0xFF) * ir[k + 1]) << 4;
        if ((op & 0x3F) == 0x14)
            depth_cue(m);
        finish_color(m);
        break;
    case 0x29: /* DCPL */
        for (k = 0; k < 3; k++)
            m[k] = ((long long)(rgbc >> (8 * k) & 0xFF) * ir[k + 1]) << 4;
        depth_cue(m);
        finish_color(m);
        break;
    case 0x28: /* SQR */
        for (k = 1; k <= 3; k++)
            set_mac_ir(k, (long long)ir[k] * ir[k]);
        break;
    case 0x2D:
    case 0x2E: {
        long long sum = (op & 0x3F) == 0x2D ? (long long)zsf3 * (sz[1] + sz[2] + sz[3])
                                            : (long long)zsf4 * (sz[0] + sz[1] + sz[2] + sz[3]);

        mac[0] = (int)check_mac(0, sum);
        otz = saturate((int)(sum >> 12), 0, 0xFFFF, 1 << 18);
        break;
    }
    case 0x3D: /* GPF */
    case 0x3E: /* GPL */
        for (k = 1; k <= 3; k++)
            set_mac_ir(k, ((op & 0x3F) == 0x3E ? (long long)mac[k] << (sf * 12) : 0) + (long long)ir[k] * ir[0]);
        push_color();
        break;
    }
    if (flag & FLAG_ERROR_BITS)
        flag |= 0x80000000u;
}

static unsigned int rgb15_from_ir(void)
{
    unsigned int out = 0;
    int k, c;

    for (k = 0; k < 3; k++) {
        c = ir[k + 1] >> 7;
        out |= (unsigned int)(c < 0 ? 0 : c > 31 ? 31 : c) << (5 * k);
    }
    return out;
}

unsigned int gte_read(int reg)
{
    unsigned int x;
    int n = 0;

    switch (reg) {
    case 0: case 2: case 4:
        return pack(v[reg / 2][0], v[reg / 2][1]);
    case 1: case 3: case 5:
        return v[reg / 2][2];
    case 6:
        return rgbc;
    case 7:
        return otz;
    case 8: case 9: case 10: case 11:
        return ir[reg - 8];
    case 12: case 13: case 14:
        return sxy[reg - 12];
    case 15:
        return sxy[2];
    case 16: case 17: case 18: case 19:
        return sz[reg - 16];
    case 20: case 21: case 22:
        return rgb[reg - 20];
    case 23:
        return res1;
    case 24: case 25: case 26: case 27:
        return mac[reg - 24];
    case 28: case 29:
        return rgb15_from_ir();
    case 30:
        return lzcs;
    default: /* LZCR: leading zeroes of a positive LZCS, leading ones of a negative one */
        x = lzcs < 0 ? ~(unsigned int)lzcs : (unsigned int)lzcs;
        while (n < 32 && !(x & 0x80000000u >> n))
            n++;
        return n;
    }
}

void gte_write(int reg, unsigned int x)
{
    switch (reg) {
    case 0: case 2: case 4:
        v[reg / 2][0] = x;
        v[reg / 2][1] = x >> 16;
        break;
    case 1: case 3: case 5:
        v[reg / 2][2] = x;
        break;
    case 6:
        rgbc = x;
        break;
    case 7:
        otz = x & 0xFFFF;
        break;
    case 8: case 9: case 10: case 11:
        ir[reg - 8] = x;
        break;
    case 12: case 13: case 14:
        sxy[reg - 12] = x;
        break;
    case 15: /* SXYP pushes the FIFO */
        sxy[0] = sxy[1];
        sxy[1] = sxy[2];
        sxy[2] = x;
        break;
    case 16: case 17: case 18: case 19:
        sz[reg - 16] = x & 0xFFFF;
        break;
    case 20: case 21: case 22:
        rgb[reg - 20] = x;
        break;
    case 23:
        res1 = x;
        break;
    case 24: case 25: case 26: case 27:
        mac[reg - 24] = x;
        break;
    case 28: /* IRGB: 5-bit colour to IR1-3 */
        ir[1] = (x & 31) * 0x80;
        ir[2] = (x >> 5 & 31) * 0x80;
        ir[3] = (x >> 10 & 31) * 0x80;
        break;
    case 30:
        lzcs = x;
        break;
    }
}

static short (*matrix(int reg))[3]
{
    return reg < 5 ? rt : reg < 13 ? llm : lcm;
}

unsigned int gte_read_ctrl(int reg)
{
    short (*m)[3];
    int base;

    if (reg < 5 || (reg >= 8 && reg < 13) || (reg >= 16 && reg < 21)) {
        m = matrix(reg);
        base = (reg < 5 ? reg : reg < 13 ? reg - 8 : reg - 16) * 2; /* elements in row order, two per register */
        if (base == 8)
            return m[2][2];
        return pack(m[base / 3][base % 3], m[(base + 1) / 3][(base + 1) % 3]);
    }
    switch (reg) {
    case 5: case 6: case 7:
        return tr[reg - 5];
    case 13: case 14: case 15:
        return bk[reg - 13];
    case 21: case 22: case 23:
        return fc[reg - 21];
    case 24:
        return ofx;
    case 25:
        return ofy;
    case 26:
        return sext16(h); /* the hardware sign-extends H when it's read */
    case 27:
        return dqa;
    case 28:
        return dqb;
    case 29:
        return zsf3;
    case 30:
        return zsf4;
    default:
        return flag;
    }
}

void gte_write_ctrl(int reg, unsigned int x)
{
    short (*m)[3];
    int base;

    if (reg < 5 || (reg >= 8 && reg < 13) || (reg >= 16 && reg < 21)) {
        m = matrix(reg);
        base = (reg < 5 ? reg : reg < 13 ? reg - 8 : reg - 16) * 2;
        m[base / 3][base % 3] = x;
        if (base < 8)
            m[(base + 1) / 3][(base + 1) % 3] = x >> 16;
        return;
    }
    switch (reg) {
    case 5: case 6: case 7:
        tr[reg - 5] = x;
        break;
    case 13: case 14: case 15:
        bk[reg - 13] = x;
        break;
    case 21: case 22: case 23:
        fc[reg - 21] = x;
        break;
    case 24:
        ofx = x;
        break;
    case 25:
        ofy = x;
        break;
    case 26:
        h = x & 0xFFFF;
        break;
    case 27:
        dqa = x;
        break;
    case 28:
        dqb = x;
        break;
    case 29:
        zsf3 = x;
        break;
    case 30:
        zsf4 = x;
        break;
    case 31:
        flag = (x & 0x7FFFF000) | ((x & FLAG_ERROR_BITS) ? 0x80000000u : 0);
        break;
    }
}
