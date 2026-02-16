/* Test that deflateReset properly clears history_len
 * Targets assertion at lib/nx_deflate.c:845: assert(s->cur_in >= s->history_len)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "test.h"

#define CHUNK_SIZE 65536

int main() {
    z_stream d_strm, i_strm;
    unsigned char in[CHUNK_SIZE];
    unsigned char compressed[CHUNK_SIZE * 2];
    unsigned char decompressed[CHUNK_SIZE];
    int ret;

    for (int i = 0; i < CHUNK_SIZE; i++) {
        in[i] = (unsigned char)(i % 256);
    }

    /* Initialize with level 6 to use history */
    memset(&d_strm, 0, sizeof(d_strm));
    ret = deflateInit2(&d_strm, 6, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        fprintf(stderr, "deflateInit2 failed: %d\n", ret);
        return TEST_ERROR;
    }

    /* First compression to build history */
    d_strm.avail_in = CHUNK_SIZE;
    d_strm.next_in = in;
    d_strm.avail_out = sizeof(compressed);
    d_strm.next_out = compressed;

    ret = deflate(&d_strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        fprintf(stderr, "First deflate failed: %d\n", ret);
        deflateEnd(&d_strm);
        return TEST_ERROR;
    }

    /* Reset - must clear history_len to avoid assertion */
    ret = deflateReset(&d_strm);
    if (ret != Z_OK) {
        fprintf(stderr, "deflateReset failed: %d\n", ret);
        deflateEnd(&d_strm);
        return TEST_ERROR;
    }

    /* Second compression - triggers assertion if history_len not cleared */
    d_strm.avail_in = CHUNK_SIZE;
    d_strm.next_in = in;
    d_strm.avail_out = sizeof(compressed);
    d_strm.next_out = compressed;

    ret = deflate(&d_strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        fprintf(stderr, "Second deflate failed: %d\n", ret);
        deflateEnd(&d_strm);
        return TEST_ERROR;
    }

    unsigned long compressed_size = d_strm.total_out;
    deflateEnd(&d_strm);

    /* Inflate to verify correctness */
    memset(&i_strm, 0, sizeof(i_strm));
    ret = inflateInit(&i_strm);
    if (ret != Z_OK) {
        fprintf(stderr, "inflateInit failed: %d\n", ret);
        return TEST_ERROR;
    }

    i_strm.avail_in = compressed_size;
    i_strm.next_in = compressed;
    i_strm.avail_out = sizeof(decompressed);
    i_strm.next_out = decompressed;

    ret = inflate(&i_strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        fprintf(stderr, "inflate failed: %d\n", ret);
        inflateEnd(&i_strm);
        return TEST_ERROR;
    }

    if (i_strm.total_out != CHUNK_SIZE) {
        fprintf(stderr, "Size mismatch: expected %d, got %lu\n",
                CHUNK_SIZE, i_strm.total_out);
        inflateEnd(&i_strm);
        return TEST_ERROR;
    }

    if (memcmp(in, decompressed, CHUNK_SIZE) != 0) {
        fprintf(stderr, "Data mismatch after decompression\n");
        inflateEnd(&i_strm);
        return TEST_ERROR;
    }

    inflateEnd(&i_strm);
    printf("%s passed\n", __FILE__);
    return TEST_OK;
}
