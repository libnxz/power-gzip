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
    // why?
    if (sum2 >= ((unsigned long)BASE << 1)) sum2 -= ((unsigned long)BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}


/* ========================================================================= */
unsigned long new_adler32_combine(adler1, adler2, len2)
    unsigned long adler1;
    unsigned long adler2;
    uint64_t len2; /* z_off64_t len2; */
{
    unsigned long sum1;
    unsigned long sum2;
    unsigned rem;

#define MYMOD(X) ((X)%BASE)
    // https://en.wikipedia.org/wiki/Adler-32
    /* subract 1 because a = 1 initially */
    //sum1 = MYMOD( (adler1 & 0xffff) + (adler2 & 0xffff) - 1 );
    sum1 = (adler1 & 0xffff) + (adler2 & 0xffff) + BASE - 1;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;    
    
    //sum1 = MYMOD( (adler1 & 0xffff) + (adler2 & 0xffff) + BASE - 1 );    
    // a is added to b len2 times
    sum2 = MYMOD( MYMOD(len2) * (adler1 & 0xffff) );
    //sum2 = MYMOD( len2 * (adler1 & 0xffff) );
    sum2 = sum2 + ((adler1 >> 16) & 0xffff) + ((adler2 >> 16) & 0xffff) + BASE - MYMOD(len2) ;
    sum2 = sum2 % (2*BASE);
    sum2 = MYMOD(sum2);

    return sum1 | (sum2 << 16);
}

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

int main()
{
  long a,b,len;
#if 0  
  for(a=1; a<(10*BASE); a++)
    for(b=1; b<(10*BASE); b++)
      for(len=1; len<1000; len++) {
	assert( nx_adler32_combine(a,b,len) ==  new_adler32_combine(a,b,len) );
	// printf("%lx %lx %ld\n", nx_adler32_combine(a,b,len), new_adler32_combine(a,b,len), len );
      }
#else
  while(1) {
    a = 0xFFFFFFFFUL & lrand48();
    b = 0xFFFFFFFFUL & lrand48();
    len = 0xFFFFFFFUL & lrand48();
    if( nx_adler32_combine(a,b,len) !=  new_adler32_combine(a,b,len) )
      printf("%lx %lx %lx %lx %ld\n", nx_adler32_combine(a,b,len), new_adler32_combine(a,b,len), a, b, len );      
  }
#endif
  
  return 0;
}

