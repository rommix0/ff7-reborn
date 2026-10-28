/* Small word/bit helpers in the main exe at 0x80014A00-0x80014A84. Built with GCC 2.7.2 -O2 -G0. */

void copy_words(int *dst, int *src, int bytes)
{
    int i;

    bytes >>= 2;
    for (i = 0; i < bytes; i++)
        *dst++ = *src++;
}

/* Index of the highest set bit (0 for 0 and 1). The goto shape is what reproduces the original's unrotated loop. */
int highest_bit(unsigned int x)
{
    int n = 0;

loop:
    x >>= 1;
    if (x != 0) {
        n++;
        goto loop;
    }
    return n;
}

int count_bits(unsigned int x)
{
    int n = 0;

    while (x) {
        if (x & 1)
            n++;
        x >>= 1;
    }
    return n;
}
