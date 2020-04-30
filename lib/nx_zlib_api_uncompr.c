/* Function interfaces derived and modified from zlib uncompr.c.  */

/*
  Copyright (C) 1995-2003, 2010, 2014, 2016 Jean-loup Gailly, Mark Adler

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

int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
	return nx_uncompress(dest, destLen, source, sourceLen);
}
int uncompress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong *sourceLen)
{
	return nx_uncompress2(dest, destLen, source, sourceLen);
}
