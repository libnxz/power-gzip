/* nx_gzlib.c -- implement gz* functions
 *
 * NX-GZIP compression accelerator user library
 * implementing zlib compression library interfaces
 *
 * Copyright (C) IBM Corporation, 2022
 *
 * Licenses for GPLv2 and Apache v2.0:
 *
 * GPLv2:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Apache v2.0:
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/** @file nx_gzlib.c
 * \brief Implement the gz* functions for the NX GZIP accelerator and
 * related functions.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include "nx_zlib.h"

#define BUF_LEN 1024
#define DEF_MEM_LEVEL 8
#define MAX_LEN 4096

bool is_deflate;

typedef struct gz_state {
	gzFile gz;
	int fd;
	FILE *fp;
	int err;
	z_stream strm;
	uint8_t * buf;
	int32_t used_buf;
	uint8_t * cur_buf;
} *gz_statep;

int __gzclose(gzFile file, int force_nx)
{
	gz_statep state = (gz_statep) file;
	z_streamp stream = &(state->strm);
	int ret;
	unsigned char* next_out;

	if (file == NULL){
		errno = EINVAL;
		return Z_STREAM_ERROR;
	}

	next_out = malloc(sizeof(char)*MAX_LEN);
	if (next_out == NULL)
		return Z_MEM_ERROR;

	if (is_deflate) {
		typeof(deflate) (*def);
		typeof(deflateEnd) (*defEnd);
		if (force_nx) {
			def = &nx_deflate;
			defEnd = &nx_deflateEnd;
		} else {
			def = &deflate;
			defEnd = &deflateEnd;
		}

		/* Finish the stream.  */
		stream->next_in = Z_NULL;
		stream->avail_in = 0;
		ssize_t w_ret = 0;
		do {
			stream->next_out = next_out;
			stream->avail_out = MAX_LEN;

			ret = (*def)(stream, Z_FINISH);

			w_ret = write(state->fd, next_out,
				      MAX_LEN - stream->avail_out);
		} while (ret != Z_STREAM_END && w_ret >= 0);

		ret = (*defEnd)(stream);
	} else {
		if (force_nx)
			ret = nx_inflateEnd(stream);
		else
			ret = inflateEnd(stream);
	}

	if(state->fp == NULL)
		close(state->fd);
	else
		fclose(state->fp);

	if (state->buf != NULL)
		free(state->buf);
	free(next_out);
	free(state);

	return ret;
}

int nx_gzclose(gzFile file)
{
	return __gzclose(file, 1);
}

gzFile __gzopen(const char* path, int fd, const char *mode, int force_nx)
{
	gz_statep state;
	gzFile file;
	int err, strategy = Z_DEFAULT_STRATEGY, level = Z_DEFAULT_COMPRESSION;
	char* digit;
	void* stream;

	state = malloc(sizeof(struct gz_state));
	if (state == NULL)
		return NULL;
	memset(state, 0, sizeof(struct gz_state));

	if (path == NULL){
		state->fp = NULL;
		if(fdopen(fd, mode) != NULL)
			state->fd = fd;
		else
			state->fd = -1;
	} else {
		state->fp = fopen(path, mode);
		state->fd = fileno(state->fp);
	}

	if (state->fd == -1){
		free(state);
		return NULL;
	}

	stream = &(state->strm);
	memset(stream, 0, sizeof(z_stream));
	if (strchr(mode, 'w')){
		if (strchr(mode, 'h'))
			strategy = Z_HUFFMAN_ONLY;
		else if (strchr(mode, 'f'))
			strategy = Z_FILTERED;
		else if (strchr(mode, 'R'))
			strategy = Z_RLE;

		digit = strpbrk(mode, "0123456789");
		if (digit != NULL)
			level = atoi(digit);

		if (force_nx)
			err = nx_deflateInit2(stream, level, Z_DEFLATED, 31,
					      DEF_MEM_LEVEL, strategy);
		else
			err = deflateInit2(stream, level, Z_DEFLATED, 31,
					   DEF_MEM_LEVEL, strategy);

		is_deflate = true;
	} else {
		if (force_nx)
			err = nx_inflateInit(stream);
		else
			err = inflateInit(stream);
		is_deflate = false;
		if (err == Z_OK) {
			state->buf = malloc(BUF_LEN);
			if (state->buf == NULL) {
				err=Z_NULL;
			}
		}
	}
	if (err != Z_OK){
		free(state);
		if (err == Z_STREAM_ERROR)
			errno = EINVAL;
		return NULL;
	}

	file = (gzFile) state;
	return file;
}

gzFile nx_gzdopen(int fd, const char *mode)
{
	return __gzopen(NULL, fd, mode, 1);
}

gzFile nx_gzopen(const char* path, const char *mode)
{
	return __gzopen(path, 0, mode, 1);
}

int __gzread(gzFile file, void *buf, unsigned len, int force_nx)
{
	gz_statep state = (gz_statep) file;
	z_streamp stream = &(state->strm);
	int ret, err;
	const uInt max = len > 10? 10 : 1;
	uLong last_total_out = stream->total_out;
	typeof(inflate) (*inf);

	if (force_nx)
		inf = &nx_inflate;
	else
		inf = &inflate;

	stream->next_out = buf;
	stream->avail_out = len;
	do {
		if (state->used_buf == 0) {
			ret = read(state->fd, state->buf, max);
			stream->next_in = state->buf;
		} else {
			/* There are still data pending in the buffer. */
			ret = state->used_buf;
			stream->next_in = state->cur_buf;
		}
		if (ret == 0){
			/* Flush all at the end of the file. */
			err = (*inf)(stream, Z_FINISH);
			break;
		}

		stream->avail_in = ret;
		err = (*inf)(stream, Z_NO_FLUSH);

		state->used_buf = stream->avail_in;
		state->cur_buf  = stream->next_in;
		if (err != Z_OK && err != Z_STREAM_END){
			state->err = err;
			return 0;
		}
	} while (stream->avail_out != 0);

	return stream->total_out - last_total_out;
}

int nx_gzread(gzFile file, void *buf, unsigned len) {
	return __gzread(file, buf, len, 1);
}

/** \brief Write to a compressed file. Internal implementation.
 *
 *  @param file gzip file descriptor to be processed.
 *  @param buf Buffer containing uncompressed data.
 *  @param len Amount of bytes in buf.
 *  @param force_nx True if NX should be enforced, otherwise fallback to
 *                  configuration settings.
 *  @return On success, return the number of uncompressed bytes processed from
 *          buf. On error, return a value less or equal to 0.
 */
int __gzwrite (gzFile file, const void *buf, unsigned len, int force_nx)
{
	gz_statep state = (gz_statep) file;
	z_streamp stream = &(state->strm);
	int rc;
	uLong last_total_in = stream->total_in;
	unsigned char* next_out = malloc(len);
	if (next_out == NULL)
		return Z_MEM_ERROR;

	/* Under some scenarios (e.g. when compressing random data), the
	   output will be larger than the input.  While the NX can buffer some
	   output data that didn't fit in next_out, if the amount of input data
	   is large enough, the amount of data cached in the output buffer will
	   keep increasing reaching a point where the amount of cached output
	   is equal to len.  The following loop is meant to prevent a scenario
	   where no input will be processed causing nx_gzwrite to return 0,
	   which is considered to be an error.  */
	stream->next_in = (z_const Bytef *)buf;
	stream->avail_in = len;
	do {
		stream->next_out = next_out;
		stream->avail_out = len;

		if (force_nx)
			rc = nx_deflate(stream, Z_NO_FLUSH);
		else
			rc = deflate(stream, Z_NO_FLUSH);

		ssize_t w_ret = write(state->fd, next_out,
				      len - stream->avail_out);

		if (w_ret < 0) {
			free(next_out);
			state->err = Z_ERRNO;
			return 0;
		}
		if (rc != Z_OK && rc != Z_STREAM_END){
			free(next_out);
			state->err = rc;
			return 0;
		}
	} while (stream->total_in == last_total_in);

	free(next_out);
	return stream->total_in - last_total_in;
}

int nx_gzwrite (gzFile file, const void *buf, unsigned len) {
	return __gzwrite(file, buf, len, 1);
}

int gzclose(gzFile file)
{
	return __gzclose(file, 0);
}

gzFile gzopen(const char *path, const char *mode)
{
	return __gzopen(path, 0, mode, 0);
}

gzFile gzdopen(int fd, const char *mode)
{
	return __gzopen(NULL, fd, mode, 0);
}

int gzread(gzFile file, void *buf, unsigned len)
{
	return __gzread(file, buf, len, 0);
}

int gzwrite(gzFile file, const void *buf, unsigned len)
{
	return __gzwrite(file, buf, len, 0);
}
