/* The game's random numbers, 0x80014A84-0x80014C70: a 256-byte table read through eight cursors, the one in use
 * moving on now and then. Built with -G8, like loader.c: the cursor index and the draw counter are reached through
 * $gp, so they're tentative definitions here, left common for the linker to resolve to the data section's labels. */

int D_80062E18;                     /* which cursor draws next, 0-7 */
extern unsigned char D_80062E10[];  /* the eight cursors' places in the table */
extern unsigned char D_80083084[];  /* the table */
int D_80062D4C;                     /* 16-bit draws so far */

int count_bits(unsigned int x);
#ifdef NATIVE
unsigned char rand_byte(void); /* a modern compiler won't take an implicit int and then this */
#endif

/* One of mask's set bits, at random (0 for an empty mask). rand_byte isn't declared yet here, so its result is used
 * unmasked, as in the game. */
unsigned int rand_bit(unsigned int mask)
{
    int n;
    unsigned int bit = 0;

    n = count_bits(mask);
    if (n != 0) {
        n = rand_byte() * n >> 8;
        for (bit = 1; bit != 0; bit <<= 1) {
            if (mask & bit && --n < 0)
                break;
        }
    }
    return bit;
}

/* value moved by step towards max (up) or 0, stopping there, wrap-around included. */
unsigned int step_clamped(unsigned int value, unsigned int step, unsigned int max, int up)
{
    unsigned int old = value;

    if (up) {
        value += step;
        if (value < old)
            value = max;
        if (max < value)
            value = max;
    } else {
        value -= step;
        if (old < value)
            value = 0;
    }
    return value;
}

void rand_next_cursor(void)
{
    D_80062E18 = (D_80062E18 + 1) & 7;
}

unsigned char rand_byte(void)
{
    return D_80083084[D_80062E10[D_80062E18]++];
}

/* 0 to n - 1, n at most 256. */
unsigned char rand_below(int n)
{
    return rand_byte() * n >> 8;
}

/* 16 bits; seven draws in eight move on to the next cursor between the two bytes. */
int rand_u16(void)
{
    unsigned char low = rand_byte();
    int high;

    if (D_80062D4C++ & 7)
        rand_next_cursor();
    high = rand_byte() << 8;
    return low | high;
}

/* Each cursor starts at the seed shifted right by its number. */
void rand_seed(int seed)
{
    int i;

    for (i = 0; i < 8; i++) {
        D_80062E10[i] = seed;
        seed >>= 1;
    }
    D_80062E18 = 0;
}
