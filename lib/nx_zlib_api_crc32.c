/* CRC-32 zlib function interfaces, derived and modified from zlib
   crc32.c.  */

/*
  Copyright (C) 1995-2006, 2010, 2011, 2012, 2016 Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
  (zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
*/

#include <zlib.h>
#include "nx_zlib.h"
#include "crc32_ppc.h"

unsigned long crc32(unsigned long crc, const unsigned char FAR *buf,
		    uInt len)
{
    return crc32_ppc(crc, (unsigned char *)buf, len);
}

uLong crc32_combine(uLong crc1, uLong crc2, z_off_t len2)
{
    return nx_crc32_combine(crc1, crc2, len2);
}

uLong nx_crc32_combine64(uLong crc1, uLong crc2, uint64_t len2);

uLong crc32_combine64(uLong crc1, uLong crc2, z_off64_t len2)
{
    return nx_crc32_combine64(crc1, crc2, len2);
}
