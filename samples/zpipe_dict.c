/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
 */

/*  
    Example:

    Make a small file fragment that normally wouldn't compress well
    head -c 50 zlib_how.html > small_file
    tail -c 50 zlib_how.html >> small_file
    ls -l small_file
    -rw-rw-r-- 1 abalib abalib 100 Dec 12 15:38 small_file

    Compress it and check the size. Notice that the file actually expanded
    gzip small_file
    ls -l small_file.gz
    -rw-rw-r-- 1 abalib abalib 124 Dec 12 15:38 small_file.gz

    Let's try using a dictionary to compressed the same file.
    I will use the original file as a dictionary, a bit unrealistic
    nevertheless demonstrates the idea:
    ./zpipe_dict zlib_how.html < small_file > junk.z

    Note the new size, 19 bytes. Compare that to the original size of 100
    bytes. It's a factor of 5x thanks to the dictionary
    ls -l junk.z
    -rw-rw-r-- 1 abalib abalib 19 Dec 12 15:42 junk.z

    Now verify that the file can be decompressed correctly
    ./zpipe_dict zlib_how.html -d <  junk.z > junk
    sum junk small_file
    08615     1 junk
    08615     1 small_file

*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

#define CHECK_ERR(err, msg) do {if (err != Z_OK) { fprintf(stderr, "%s error: %d\n", msg, err); }} while(0)

#define DBGDICT(X) do { X;}while(0)

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int def(FILE *source, FILE *dest, FILE *dictfp, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    unsigned char dictionary[32769];
    int dict_len;

    dict_len = fread(dictionary, 1, 32768, dictfp);
    if (ferror(dictfp)) {
	perror("cannot read dictionary");
	return Z_ERRNO;
    }

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(&strm,
                       Z_DEFAULT_COMPRESSION, /* Z_BEST_COMPRESSION, */
                       Z_DEFLATED,
		       15, /* zlib format */
                       8,
                       Z_DEFAULT_STRATEGY );  /* Z_FIXED ); //Z_DEFAULT_STRATEGY ); */
    if (ret != Z_OK)
        return ret;

    /* DBGDICT( fprintf(stderr, "adler32 %08lx after %s\n", strm.adler, "deflateInit") ); */

    /* insert the dictionary */
    ret = deflateSetDictionary(&strm, dictionary, dict_len);
    if (ret != Z_OK) {
	CHECK_ERR(ret, "deflateSetDictionary");
	return ret;
    }

    fprintf(stderr, "deflate dictId %08lx\n", strm.adler);
    
    DBGDICT( fprintf(stderr, "adler32 %08lx after %s\n", strm.adler, "deflateSetDictionary") );

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    DBGDICT( fprintf(stderr, "adler32 %08lx after %s\n", strm.adler, "last deflate") );

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest, FILE *dictfp)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    unsigned char dictionary[32769];
    int dict_len;
    uLong dictId;

    dict_len = fread(dictionary, 1, 32768, dictfp);
    if (ferror(dictfp)) {
	perror("cannot read dictionary");
	return Z_ERRNO;
    }

    /* check the ID of the external dictionary */
    dictId = adler32(0L, Z_NULL, 0);
    dictId = adler32(dictId, dictionary, dict_len);
    fprintf(stderr, "dict file has dictId %08lx\n", dictId);    
    
    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* DBGDICT( fprintf(stderr, "adler32 %08lx after %s\n", strm.adler, "inflateInit") ); */

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

            if (ret == Z_NEED_DICT) {
		DBGDICT( fprintf(stderr, "compressed file contains dictId %08lx\n", strm.adler) );
		ret = inflateSetDictionary(&strm, dictionary, dict_len);
		if (ret != Z_OK) {
		    CHECK_ERR(ret, "inflateSetDictionary");
		    return ret;
		}

		DBGDICT( fprintf(stderr, "adler32 %08lx after %s\n", strm.adler, "inflateSetDictionary") );
		ret = inflate(&strm, Z_NO_FLUSH);
		assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
		DBGDICT( fprintf(stderr, "adler32 %08lx after %s\n", strm.adler, "inflate") );
	    }

	    else if ((ret == Z_DATA_ERROR) || (ret == Z_MEM_ERROR )) {
		inflateEnd(&strm);
		return ret;
	    }

            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
		(void)inflateEnd(&strm);
		return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe_dict: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

void print_usage()
{
    fputs("usage: zpipe_dict dictionary.file [-d] < source > dest.z\n", stderr);
    fputs("       dest.z cannot be decompressed correctly without a matching dictionary\n", stderr);
}

/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret;
    FILE *dictfp;
    /* avoid end-of-line conversions */
    SET_BINARY_MODE(stdin);
    SET_BINARY_MODE(stdout);

    dictfp = fopen(argv[1], "r");
    if (dictfp == NULL) {
	perror("cannot open dictionary.file");
	print_usage();
	return -1;
    }
    
    /* do compression if no arguments */
    if (argc == 2) {
	ret = def(stdin, stdout, dictfp, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK)
            zerr(ret);
        return ret;
    }

    /* do decompression if -d specified */
    else if (argc == 3 && strcmp(argv[2], "-d") == 0) {
        ret = inf(stdin, stdout, dictfp);
        if (ret != Z_OK)
            zerr(ret);
        return ret;
    }

    /* otherwise, report usage */
    else {
	print_usage();
        return 1;
    }
}
