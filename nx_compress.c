#include <zlib.h>
#include "nx_zlib.h"

int nx_compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
    z_stream stream;
    int err;
    const uInt max = (uInt)-1;
    uLong left;

    left = *destLen;
    *destLen = 0;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    prt_info("nx_compress2 begin: sourceLen %d\n", sourceLen);

    err = nx_deflateInit(&stream, level);
    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;
    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;
    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = sourceLen > (uLong)max ? max : (uInt)sourceLen;
            sourceLen -= stream.avail_in;
        }
        err = nx_deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
	prt_info("     err %d\n", err);
    } while (err == Z_OK);

    *destLen = stream.total_out;
    nx_deflateEnd(&stream);

    prt_info("nx_compress2 end: destLen %d\n", *destLen);
    return err == Z_STREAM_END ? Z_OK : err;
}

int nx_compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    return nx_compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

uLong nx_compressBound(uLong sourceLen)
{
    return sourceLen * 15/8 + NX_MIN( sysconf(_SC_PAGESIZE), 1<<16 );
    // return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) +
    //        (sourceLen >> 25) + 13;
}

#ifdef ZLIB_API

int compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
	return nx_compress(dest, destLen, source, sourceLen);
}
int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
	return nx_compress2(dest, destLen, source, sourceLen, level);
}
uLong compressBound(uLong sourceLen)
{
	return nx_compressBound(sourceLen);
}

#endif

