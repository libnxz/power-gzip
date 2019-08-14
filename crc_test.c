/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of data at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 *
 * Copyright (C) 1995-2011, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Changes:
 *    Standalone compile made possible.
 *    External functions prefixed with nx_* to prevent zlib name collisions.
 *
 */

/* @(#) $Id$ */

/*
  Note on the use of DYNAMIC_CRC_TABLE: there is no mutex or semaphore
  protection on the static variables used to control the first-use generation
  of the crc tables.  Therefore, if you #define DYNAMIC_CRC_TABLE, you should
  first call get_crc_table() to initialize the tables before allowing more than
  one thread to use crc32().

  DYNAMIC_CRC_TABLE and MAKECRCH can be #defined to write out crc32.h.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>  /* ptrdiff_t */
#include <byteswap.h>


    
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>


/* make crc32.c work without external zlib headers */
#undef MAKECRCH
#undef DYNAMIC_CRC_TABLE
#define FAR
#define local static
#define OF(args)  ()
typedef unsigned long uLong;
typedef unsigned int uInt;
typedef uint32_t z_crc_t;
typedef size_t   z_size_t;
typedef off_t    z_off_t;
typedef off_t    z_off64_t;

#undef NOBYFOUR
#define Z_U4 uint32_t
#define ZEXPORT
#define ZEXTERN extern
#define Z_NULL NULL
#define ZSWAP32(q) bswap_32(q)


#ifdef MAKECRCH
#  include <stdio.h>
#  ifndef DYNAMIC_CRC_TABLE
#    define DYNAMIC_CRC_TABLE
#  endif /* !DYNAMIC_CRC_TABLE */
#endif /* MAKECRCH */

/* #include "zutil.h"      /* for STDC and FAR definitions */

/* Definitions for doing the crc four data bytes at a time. */
#if !defined(NOBYFOUR) && defined(Z_U4)
#  define BYFOUR
#endif
#ifdef BYFOUR
   local unsigned long crc32_little OF((unsigned long,
                        const unsigned char FAR *, z_size_t));
   local unsigned long crc32_big OF((unsigned long,
                        const unsigned char FAR *, z_size_t));
#  define TBLS 8
#else
#  define TBLS 1
#endif /* BYFOUR */

/* Local functions for crc concatenation */
local unsigned long gf2_matrix_times OF((unsigned long *mat,
                                         unsigned long vec));
local void gf2_matrix_square OF((unsigned long *square, unsigned long *mat));
local uLong crc32_combine_ OF((uLong crc1, uLong crc2, z_off64_t len2));


#ifdef DYNAMIC_CRC_TABLE

local volatile int crc_table_empty = 1;
local z_crc_t FAR crc_table[TBLS][256];
local void make_crc_table OF((void));
#ifdef MAKECRCH
   local void write_table OF((FILE *, const z_crc_t FAR *));
#endif /* MAKECRCH */
/*
  Generate tables for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The first table is simply the CRC of all possible eight bit values.  This is
  all the information needed to generate CRCs on data a byte at a time for all
  combinations of CRC register values and incoming bytes.  The remaining tables
  allow for word-at-a-time CRC calculation for both big-endian and little-
  endian machines, where a word is four bytes.
*/
local void make_crc_table()
{
    z_crc_t c;
    int n, k;
    z_crc_t poly;                       /* polynomial exclusive-or pattern */
    /* terms of polynomial defining this crc (except x^32): */
    static volatile int first = 1;      /* flag to limit concurrent making */
    static const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

    /* See if another task is already doing this (not thread-safe, but better
       than nothing -- significantly reduces duration of vulnerability in
       case the advice about DYNAMIC_CRC_TABLE is ignored) */
    if (first) {
        first = 0;

        /* make exclusive-or pattern from polynomial (0xedb88320UL) */
        poly = 0;
        for (n = 0; n < (int)(sizeof(p)/sizeof(unsigned char)); n++)
            poly |= (z_crc_t)1 << (31 - p[n]);

        /* generate a crc for every 8-bit value */
        for (n = 0; n < 256; n++) {
            c = (z_crc_t)n;
            for (k = 0; k < 8; k++)
                c = c & 1 ? poly ^ (c >> 1) : c >> 1;
            crc_table[0][n] = c;
        }

#ifdef BYFOUR
        /* generate crc for each value followed by one, two, and three zeros,
           and then the byte reversal of those as well as the first table */
        for (n = 0; n < 256; n++) {
            c = crc_table[0][n];
            crc_table[4][n] = ZSWAP32(c);
            for (k = 1; k < 4; k++) {
                c = crc_table[0][c & 0xff] ^ (c >> 8);
                crc_table[k][n] = c;
                crc_table[k + 4][n] = ZSWAP32(c);
            }
        }
#endif /* BYFOUR */

        crc_table_empty = 0;
    }
    else {      /* not first */
        /* wait for the other guy to finish (not efficient, but rare) */
        while (crc_table_empty)
            ;
    }

#ifdef MAKECRCH
    /* write out CRC tables to crc32.h */
    {
        FILE *out;

        out = fopen("crc32.h", "w");
        if (out == NULL) return;
        fprintf(out, "/* crc32.h -- tables for rapid CRC calculation\n");
        fprintf(out, " * Generated automatically by crc32.c\n */\n\n");
        fprintf(out, "local const z_crc_t FAR ");
        fprintf(out, "crc_table[TBLS][256] =\n{\n  {\n");
        write_table(out, crc_table[0]);
#  ifdef BYFOUR
        fprintf(out, "#ifdef BYFOUR\n");
        for (k = 1; k < 8; k++) {
            fprintf(out, "  },\n  {\n");
            write_table(out, crc_table[k]);
        }
        fprintf(out, "#endif\n");
#  endif /* BYFOUR */
        fprintf(out, "  }\n};\n");
        fclose(out);
    }
#endif /* MAKECRCH */
}

#ifdef MAKECRCH
local void write_table(out, table)
    FILE *out;
    const z_crc_t FAR *table;
{
    int n;

    for (n = 0; n < 256; n++)
        fprintf(out, "%s0x%08lxUL%s", n % 5 ? "" : "    ",
                (unsigned long)(table[n]),
                n == 255 ? "\n" : (n % 5 == 4 ? ",\n" : ", "));
}
#endif /* MAKECRCH */

#else /* !DYNAMIC_CRC_TABLE */
/* ========================================================================
 * Tables of CRC-32s of all single-byte values, made by make_crc_table().
 */
#include "inc_nx/crc32.h"
#endif /* DYNAMIC_CRC_TABLE */

/* =========================================================================
 * This function can be used by asm versions of crc32()
 */
local const z_crc_t FAR * nx_get_crc_table()
{
#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
        make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */
    return (const z_crc_t FAR *)crc_table;
}

/* ========================================================================= */
#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1

/* ========================================================================= */
local unsigned long nx_crc32_z(crc, buf, len)
    unsigned long crc;
    const unsigned char FAR *buf;
    z_size_t len;
{
    if (buf == Z_NULL) return 0UL;

#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
        make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */

#ifdef BYFOUR
    if (sizeof(void *) == sizeof(ptrdiff_t)) {
        z_crc_t endian;

        endian = 1;
        if (*((unsigned char *)(&endian)))
            return crc32_little(crc, buf, len);
        else
            return crc32_big(crc, buf, len);
    }
#endif /* BYFOUR */
    crc = crc ^ 0xffffffffUL;
    while (len >= 8) {
        DO8;
        len -= 8;
    }
    if (len) do {
        DO1;
    } while (--len);
    return crc ^ 0xffffffffUL;
}

/* ========================================================================= */
unsigned long nx_crc32(crc, buf, len)
    unsigned long crc;
    const unsigned char FAR *buf;
    uint64_t len;
{
    return nx_crc32_z(crc, buf, len);
}

#ifdef BYFOUR

/*
   This BYFOUR code accesses the passed unsigned char * buffer with a 32-bit
   integer pointer type. This violates the strict aliasing rule, where a
   compiler can assume, for optimization purposes, that two pointers to
   fundamentally different types won't ever point to the same memory. This can
   manifest as a problem only if one of the pointers is written to. This code
   only reads from those pointers. So long as this code remains isolated in
   this compilation unit, there won't be a problem. For this reason, this code
   should not be copied and pasted into a compilation unit in which other code
   writes to the buffer that is passed to these routines.
 */

/* ========================================================================= */
#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4

/* ========================================================================= */
local unsigned long crc32_little(crc, buf, len)
    unsigned long crc;
    const unsigned char FAR *buf;
    z_size_t len;
{
    register z_crc_t c;
    register const z_crc_t FAR *buf4;

    c = (z_crc_t)crc;
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
        len--;
    }

    buf4 = (const z_crc_t FAR *)(const void FAR *)buf;
    while (len >= 32) {
        DOLIT32;
        len -= 32;
    }
    while (len >= 4) {
        DOLIT4;
        len -= 4;
    }
    buf = (const unsigned char FAR *)buf4;

    if (len) do {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
    } while (--len);
    c = ~c;
    return (unsigned long)c;
}

/* ========================================================================= */
#define DOBIG4 c ^= *buf4++; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

/* ========================================================================= */
local unsigned long crc32_big(crc, buf, len)
    unsigned long crc;
    const unsigned char FAR *buf;
    z_size_t len;
{
    register z_crc_t c;
    register const z_crc_t FAR *buf4;

    c = ZSWAP32((z_crc_t)crc);
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
        len--;
    }

    buf4 = (const z_crc_t FAR *)(const void FAR *)buf;
    while (len >= 32) {
        DOBIG32;
        len -= 32;
    }
    while (len >= 4) {
        DOBIG4;
        len -= 4;
    }
    buf = (const unsigned char FAR *)buf4;

    if (len) do {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
    } while (--len);
    c = ~c;
    return (unsigned long)(ZSWAP32(c));
}

#endif /* BYFOUR */

#define GF2_DIM 32      /* dimension of GF(2) vectors (length of CRC) */

/* ========================================================================= */
local unsigned long gf2_matrix_times(mat, vec)
    unsigned long *mat;
    unsigned long vec;
{
    unsigned long sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

/* ========================================================================= */
local void gf2_matrix_square(square, mat)
    unsigned long *square;
    unsigned long *mat;
{
    int n;

    for (n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/* ========================================================================= */
local uLong crc32_combine_(crc1, crc2, len2)
    uLong crc1;
    uLong crc2;
    z_off64_t len2;
{
    int n;
    unsigned long row;
    unsigned long even[GF2_DIM];    /* even-power-of-two zeros operator */
    unsigned long odd[GF2_DIM];     /* odd-power-of-two zeros operator */

    /* degenerate case (also disallow negative lengths) */
    if (len2 <= 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
    row = 1;
    for (n = 1; n < GF2_DIM; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}

/* ========================================================================= */
uLong ZEXPORT nx_crc32_combine(crc1, crc2, len2)
    uLong crc1;
    uLong crc2;
    uint64_t len2;
{
    return crc32_combine_(crc1, crc2, len2);
}

uLong ZEXPORT nx_crc32_combine64(crc1, crc2, len2)
    uLong crc1;
    uLong crc2;
    uint64_t len2;
{
    return crc32_combine_(crc1, crc2, len2);
}

#ifdef ZLIB_API
unsigned long crc32(crc, buf, len)
	unsigned long crc;
	const unsigned char FAR *buf;
	uint64_t len;
{
	return nx_crc32(crc, buf, len);
}

#endif


#include <string.h>

uLong ZEXPORT new_crc32_combine(crc1, crc2, len2)
    uLong crc1;
    uLong crc2;
    uint64_t len2;
{
  char buf[1000000];
  uLong tmp;

  memset(buf,0,len2);//zero out

  // crc32 inits crc with 0xffffffff and then xors the final
  // value with 0xffffffff; we need to undo the contribs of these
  // init and last 0xffffffff;
  // Basically use the 
  // crc32(msg1)XOR 0xffffffff as the initial value of crc(msg2)
  // calculation. 
  
  // remove the 0xffffffff at the end of crc1 and crc2
  crc1 ^= 0xffffffffUL;
  crc2 ^= 0xffffffffUL;  
  // remove the 0xffffffff initial value of crc2
  buf[0]= buf[1]=buf[2]=buf[3]=0xff;
  tmp = nx_crc32(0x0,buf,len2);
  crc2 = crc2 ^ tmp; // 0xff... removed

  // now adding crc1' as the initial value of 2nd message
  // this is endian sensitive
  buf[3]= (crc1 & 0xff000000UL)>>24;
  buf[2]= (crc1 & 0x00ff0000UL)>>16;
  buf[1]= (crc1 & 0x0000ff00UL)>> 8;
  buf[0]= (crc1 & 0x000000ffUL)>> 0;
  // compute the crc of new initial value padded with zeros
  tmp = nx_crc32(0x0,buf,len2);

  // xor the new initial value in to crc2;  
  crc2 = crc2 ^ tmp;

  // Finally the tail  
  crc2 = crc2 ^ 0xffffffffUL;

  //!!! works

  // Now I need to figure out the padded with zeros algorithm.
  // I cheated above by filling a buffer with zeros which makes
  // the time linear in buffer size.  There is a faster
  // algorithm.
  
  return crc2;
}


#include "nx_crc32_tab.h"

/* utility for producing a header file; call this from main
   and redirect its output to nx_crc32_tab.h */
static int nx_make_crc_table(uint32_t *tab)
{
    /* https://en.wikipedia.org/wiki/Cyclic_redundancy_check#CRC-32_algorithm */

    printf("#ifndef _NX_CRC32_TAB_H_\n");
    printf("#define _NX_CRC32_TAB_H_\n");            
    printf("static uint32_t nx_crc32_tab[] = {\n");    
    
    for (uint32_t i = 0; i < 256; i++) {
	uint32_t crc = i;
	for (int j = 0; j < 8; j++)
	    if (crc & 1)
		crc = 0xedb88320u ^ (crc >> 1);
	    else
		crc = crc >> 1;
	printf("0x%08x, /* %3d */\n", crc, i);
	if (tab != NULL)
	    tab[i] = crc;
    }

    printf("}; /* static uint32_t nx_crc32_tab[] */\n");        
}

unsigned long new_crc32b(unsigned long crc, char *buf, size_t len)
{
    unsigned long byte;
    /* initialize */
    crc = crc ^ 0xffffffffu;
    while (len-- > 0) {
	byte = *buf++ & 0xff;
	byte = (byte ^ crc) & 0xff;
	crc = nx_crc32_tab[byte] ^ (crc >> 8);
    }
    /* finalize */
    crc ^= 0xffffffffu;
    return crc;
}


#define assert_little_endian  do { volatile int endian = 0x01234567; assert( *(char *)&endian == 0x67 ); } while(0)

/* https://en.wikipedia.org/wiki/Cyclic_redundancy_check#CRC-32_algorithm
   It is slow. But we use this only for the gzip header which is
   typically tens of bytes long.
*/
unsigned long new_crc32c(unsigned long crc, char *buf, size_t len)
{
    unsigned long byte;

    assert_little_endian;

    /* initialize */
    crc = crc ^ 0xffffffffu;
    while (len-- > 0) {
	byte = *buf++ & 0xff;
	crc = byte ^ crc; /* for big endian? */
	for (int i = 0; i < 8; i++)
	    if (crc & 1)
		crc = 0xedb88320u ^ (crc >> 1);
	    else
		crc = crc >> 1;
    }
    /* finalize */
    crc ^= 0xffffffffu;
    return crc;
}

void nx_mat_init(uint32_t *inout)
{
    for (int i = 0; i < 32; i++)
	inout[i] = 0;
}

void nx_mat_copy(const uint32_t *in, uint32_t *out)
{
    for (int i = 0; i < 32; i++)
	out[i] = in[i];    
}

/* transpose a 32b by 32b matrix.  mout and min can be the same
   pointer */
void nx_transpose(const uint32_t *min, uint32_t *mout)
{
    uint32_t m[32];
    nx_mat_init(m);
    for (int i = 0; i < 32; i++)
	for (int j = 0; j < 32; j++)
	    m[j] |= ((min[i] >> (31-j)) & 1) << (31-i);
    nx_mat_copy(m, mout);
}

/* out = in1 * in2. mout and in1/in2 can be the same pointer */
void nx_mat_mul(const uint32_t *min1, const uint32_t *min2, uint32_t *mout)
{
    uint32_t m[32];
    nx_mat_init(m);    
    for (int i = 0; i < 32; i++)
	for (int j = 0; j < 32; j++) {
	    if ((min1[i] >> (31-j) & 1))
		m[i] ^= min2[j];
	}
    nx_mat_copy(m, mout);    
}

/* vout = vin * mat */
uint32_t nx_vec_mat(uint32_t v, uint32_t *mat)
{
    uint32_t vout = 0;
    for (int j = 0; j < 32; j++) {
	if ((v >> (31-j) & 1))
	    vout ^= mat[j];
    }
    return vout;
}


/* Take n-th power of a 32x32 matrix using the exponentiation by
   squaring method.

   Basically M^n can be written as (M*M)^(n/2) for even n, and as
   M*(M*M)^((n-1)/2) for odd n. Recursively applying the equation
   results in a O(log(n)) step matrix exponentiation algorithm
*/
void nx_mat_power(uint32_t *in, uint32_t *out, uint64_t n)
{
    assert(n > 0);
    if (n == 1) {
	nx_mat_copy(in, out);
    }
    else if ( (n % 2) == 0 ) { /* even power */
	uint32_t t[32];
	nx_mat_mul(in, in, t);
	nx_mat_power(t, out, n / 2 );
    } else if ( (n % 2) == 1 ) { /* odd power */
	uint32_t t[32];
	nx_mat_mul(in, in, t);
	nx_mat_power(t, t, (n - 1) / 2 );
	nx_mat_mul(in, t, out); 
    }
}

/* a matrix for computing the new crc of appending a 0 bit to the data
   buffer given the old crc.  the first 31 row vectors are basically a
   diagonal permutation vector used for right shifting the old crc by
   1 bit. the last row vector is the crc32 polynomial xor'ed in to the
   crc if the crc least significant is 1 */
uint32_t zero_mat[32] = {
    0x40000000u, 0x20000000u, 0x10000000u, 0x08000000u,
    0x04000000u, 0x02000000u, 0x01000000u, 0x00800000u,
    0x00400000u, 0x00200000u, 0x00100000u, 0x00080000u,
    0x00040000u, 0x00020000u, 0x00010000u, 0x00008000u,
    0x00004000u, 0x00002000u, 0x00001000u, 0x00000800u,
    0x00000400u, 0x00000200u, 0x00000100u, 0x00000080u,
    0x00000040u, 0x00000020u, 0x00000010u, 0x00000008u,
    0x00000004u, 0x00000002u, 0x00000001u, 0xedb88320u,
};
    

unsigned long crc_experiment()
{
    char buf[1024];
    memset(buf, 0, 1024);
    *((uint32_t *)buf) = 0; //0x12345678;
    unsigned long crc = nx_crc32(0xffffffff, buf, 100);
    crc = crc ^ 0xffffffff; // remove the last xor
    printf("input %02x %02x %02x %02x crc %08lx\n\n", buf[0], buf[1], buf[2], buf[3], crc);

    // build adler's magic matrix
    for(int i=0; i<8; i++) {
	*((uint32_t *)buf) = 0xffffffff;
	crc = nx_crc32(0xffffffff, buf, 4);
	crc = crc ^ 0xffffffff; // remove the last xor	
	printf("input %02x %02x %02x %02x crc %08lx -- ", buf[0], buf[1], buf[2], buf[3], crc);
	buf[4] = i;
	crc = nx_crc32(0xffffffff, buf, 5);
	crc = crc ^ 0xffffffff; // remove the last xor	
	printf("input %02x %02x %02x %02x %02x crc %08lx\n", buf[0], buf[1], buf[2], buf[3], buf[4], crc);	
	
    }

    for(int i=0; i<256; i++)
      printf("crc table %d %08x\n", i, crc_table[0][i] );

    // make a 32x32 matrix (columns are uint32)
    // first column edb88320, 2nd 1 bit shifted, 3rd 2 bit shifted and so on
    // you can make those using crc32(-1,0x80,4), crc32(-1,0x40,4), crc32(-1,0x20,4) etc
    // call this the zero matrix Z.  Now we need to calculate the N-th power of Z.
    // using the squared power algorithm. we will 
    
    return crc;
}

unsigned long crc_experiment2()
{
    char buf[] = "0123456789ab";
    printf("orig crc %09lx, new crc %09lx\n", nx_crc32(0, buf, 11), new_crc32b(0, buf, 11) );
    return 0;
}


unsigned long crc_experiment3()
{
#define LMAX 256
    char buf[LMAX];
    while (1) {
	int len = lrand48() % LMAX;
	for (int i = 0; i < len; i++) {
	    buf[i] = lrand48() & 0xff;
	}
	if (nx_crc32(0, buf, len) != new_crc32c(0, buf, len)) {
	    printf("orig crc %09lx, new crc %09lx\n", nx_crc32(0, buf, len), new_crc32c(0, buf, len) );
	}
    }
    return 0;
}


unsigned long crc_experiment4()
{
#undef LMAX
#define LMAX 256
    char buf[LMAX];

    for (int i = 0; i < LMAX; i++) {
	buf[i] = 0;
    }
    
    int len = 4;
    for (int i = 0; i < len; i++) {
	buf[i] = lrand48() & 0xff;
    }
    printf("crc %08lx, +0 crc %08lx +00 crc %08lx\n", new_crc32c(0, buf, len), new_crc32c(0, buf, len+1), new_crc32c(0, buf, len+2));

    return 0;
}


void crc_experiment5()
{
    uint32_t t[32];
    uint32_t m[] = { 0x80000001, 0x80000001, 0x80000001, 0x80000000,
		     0x80000000, 0x80000000, 0x80000000, 0x80000000,
		     0x80000000, 0x80000000, 0x80000000, 0x80000000,
		     0x80000000, 0x80000000, 0x80000000, 0x80000000,
		     0x80000000, 0x80000000, 0x80000000, 0x80000000,
		     0x80000000, 0x80000000, 0x80000000, 0x80000000,
		     0x80000000, 0x80000000, 0x80000000, 0x80000000,
		     0x80000000, 0x80000001, 0x80000001, 0x8000000F, };
    nx_transpose(m, t);
    for (int i=0; i<32; i=i+4)
	printf("%08x %08x %08x %08x\n", m[i+0], m[i+1], m[i+2], m[i+3]);
    printf("\n");
    for (int i=0; i<32; i=i+4)
	printf("%08x %08x %08x %08x\n", t[i+0], t[i+1], t[i+2], t[i+3]);
}


void crc_experiment6()
{
    uint32_t t[32];
    uint32_t m1[] = { 0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000,
		      0x80000000, 0x80000000, 0x80000000, 0x80000000, };

    uint32_t m2[] = { 0xfffffffe, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000,
		      0x00000000, 0x00000000, 0x00000000, 0x00000000, };    

    nx_mat_mul(m2, m1, t);
    for (int i=0; i<32; i=i+4)
	printf("%08x %08x %08x %08x\n", m1[i+0], m1[i+1], m1[i+2], m1[i+3]);
    printf("\n");
    for (int i=0; i<32; i=i+4)
	printf("%08x %08x %08x %08x\n", m2[i+0], m2[i+1], m2[i+2], m2[i+3]);
    printf("\n");    
    for (int i=0; i<32; i=i+4)
	printf("%08x %08x %08x %08x\n", t[i+0], t[i+1], t[i+2], t[i+3]);
}

	

/* simulate appending a zero to the buffer */
unsigned long crc_experiment7()
{
    char buf[10];

    for (int i = 0; i < 10; i++) {
	buf[i] = 0;
    }
    
    int len = 4;
    for (int i = 0; i < len; i++) {
	buf[i] = lrand48() & 0xff;
    }

    uint32_t crc4byte = new_crc32c(0, buf, 4);

   
    uint32_t mat[32];
    nx_mat_init(mat);
    nx_mat_mul(zero_mat, zero_mat, mat); // 2 bits
    nx_mat_mul(mat, zero_mat, mat); //3
    nx_mat_mul(mat, zero_mat, mat); 
    nx_mat_mul(mat, zero_mat, mat); //5
    nx_mat_mul(mat, zero_mat, mat); 
    nx_mat_mul(mat, zero_mat, mat); //7
    nx_mat_mul(mat, zero_mat, mat); //8               
    uint32_t crc5byte = nx_vec_mat(crc4byte, mat);
	
    printf("crc %08lx, +0 crc %08lx, mat crc %08x\n", new_crc32c(0, buf, len), new_crc32c(0, buf, len+1), crc5byte );

    printf("static uint32_t nx_zero_byte_matrix[] = {\n");        
    for (int i=0; i<32; i=i+4)
	printf("    0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", mat[i+0], mat[i+1], mat[i+2], mat[i+3]);
    printf("}; /* static uint32_t nx_zero_byte_matrix[] */\n");            

    nx_mat_init(mat);    
    nx_mat_power(zero_mat, mat, 8);
    crc5byte = nx_vec_mat(crc4byte, mat);    

    printf("crc %08lx, +0 crc %08lx, mat crc %08x\n", new_crc32c(0, buf, len), new_crc32c(0, buf, len+1), crc5byte );
    return 0;
}



uLong ZEXPORT new_crc32_combine_b(unsigned long crc1, unsigned long crc2, uint64_t len2)
{
	uint32_t mat[32];
	uint32_t ff_and_zeros;
	uint32_t crc1_and_zeros;
	
	/* crc32 inits crc with 0xffffffff and then xors the final
	   value with 0xffffffff; we need to undo the contribs of
	   these init and last 0xffffffff; Basically, use the crc1 XOR
	   0xffffffff as the initial value of crc(msg2)
	   calculation. */
  
	/* Remove the 0xffffffff at the message ends */
	crc1 ^= 0xffffffffu;
	crc2 ^= 0xffffffffu;  

	/* Remove the 0xffffffff initial value of crc2.  Treat
	   0xffffffff as a len2 long message with (len2-4) zeros at
	   the end and compute the checksum. XOR it with crc2 which
	   removes the contrib of 0xffffffff from crc2.  Then compute
	   the checksum of crc1 and with (len2-4) zeros at the
	   end. And XOR that with crc2 which initializes crc2 with
	   crc1. */
	
	nx_mat_init(mat);

	/* matrix representing len2 zeros */
	nx_mat_power(zero_mat, mat, len2*8);

	/* zeros matrix initialized with 0xffffffff  */
	ff_and_zeros = nx_vec_mat(0xffffffffu, mat);    

	/* zeros matrix initialized with crc1 */
	crc1_and_zeros = nx_vec_mat(crc1, mat);

	/* remove 0xffffffff from crc2 (0xffffffff was the initial
	   value) and initialize the crc computation with crc1 as the
	   initial value of crc2 */
	crc2 = crc2 ^ ff_and_zeros;
	crc2 = crc2 ^ crc1_and_zeros;

	/* Add the final 0xffffffff per crc32 spec */
	crc2 = crc2 ^ 0xffffffffu;

	return crc2;
}

void crc_experiment8()
{
	uint32_t crc1, crc2, len, c1, c2;

	//for (int i=0; i<100000; i++) {
	while(1) {
		crc1 = 0xFFFFFFFFU & lrand48();
		crc2 = 0xFFFFFFFFU & lrand48();
		len = lrand48() % 10000U; //lrand48() % 1000000000U;
		c1 = nx_crc32_combine(crc1, crc2, len);
		c2 = new_crc32_combine_b(crc1, crc2, len);
		if ( c1 != c2 )
			printf("%x %x %x %x %d\n", c1, c2, crc1, crc2, len );
	}
}




int main()
{
  long a,b,len;

  //for(int i=0; i<256; i++)
  //printf("crc table %d %08x\n", i, crc_table[0][i] );

  
  //nx_make_crc_table(NULL);
  //crc_experiment3();
  //crc_experiment4();
  //crc_experiment5();
  //crc_experiment7();
  crc_experiment8();        
  return 0;

}

  
