/* 
 * Copyright (C) 1995-2011, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 * 
 * Changes:
 *   nx_adler_combine.c derived from adler32.c in zlib 1.2.11.
 *   External function names prefixed with nx_ to prevent zlib name collisions
 */

#include <stdint.h>

#define BASE 65521U     /* largest prime smaller than 65536 */
#define MOD(a) a %= BASE
#define MOD63(a) a %= BASE

/* ========================================================================= */
unsigned long nx_adler32_combine(adler1, adler2, len2)
    unsigned long adler1;
    unsigned long adler2;
    uint64_t len2; /* z_off64_t len2; */
{
    unsigned long sum1;
    unsigned long sum2;
    unsigned rem;

    /* for negative len, return invalid adler32 as a clue for debugging */
    if (len2 < 0)
        return 0xffffffffUL;

    /* the derivation of this formula is left as an exercise for the reader */
    MOD63(len2);                /* assumes len2 >= 0 */
    rem = (unsigned)len2;
    sum1 = adler1 & 0xffff;
    sum2 = rem * sum1;
    MOD(sum2);
    sum1 += (adler2 & 0xffff) + BASE - 1;
    sum2 += ((adler1 >> 16) & 0xffff) + ((adler2 >> 16) & 0xffff) + BASE - rem;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum2 >= ((unsigned long)BASE << 1)) sum2 -= ((unsigned long)BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}



