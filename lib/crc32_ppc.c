/*
 * Copyright (C) 2015 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of either:
 *
 *  a) the GNU General Public License as published by the Free Software
 *     Foundation; either version 2 of the License, or (at your option)
 *     any later version, or
 *  b) the Apache License, Version 2.0
 */
#define CRC_TABLE
#include <inttypes.h>
#include <stdlib.h>
#include <strings.h>

#include "crc32_ppc_constants.h"

#define VMX_ALIGN   16
#define VMX_ALIGN_MASK  (VMX_ALIGN-1)

static unsigned int crc32_align(unsigned int crc, const unsigned char *p,
                   unsigned long len)
{
    while (len--)
        crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return crc;
}

unsigned int __crc32_vpmsum(unsigned int crc, const unsigned char *p,
                            unsigned long len);

uint32_t crc32_ppc(uint32_t crc, const unsigned char *p, unsigned long len)
{
    unsigned int prealign;
    unsigned int tail;

    if (!p)
        return 0;

    crc ^= 0xffffffff;

    if (len < VMX_ALIGN + VMX_ALIGN_MASK) {
        crc = crc32_align(crc, p, len);
        goto out;
    }

    if ((unsigned long)p & VMX_ALIGN_MASK) {
        prealign = VMX_ALIGN - ((unsigned long)p & VMX_ALIGN_MASK);
        crc = crc32_align(crc, p, prealign);
        len -= prealign;
        p += prealign;
    }

    crc = __crc32_vpmsum(crc, p, len & ~VMX_ALIGN_MASK);

    tail = len & VMX_ALIGN_MASK;
    if (tail) {
        p += len & ~VMX_ALIGN_MASK;
        crc = crc32_align(crc, p, tail);
    }

out:
    crc ^= 0xffffffff;

    return crc;
}
