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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include "nx_zlib.h"

#define DEF_MEM_LEVEL 8
#define MAX_LEN 4096

gzFile nx_gzopen_(const char* path, int fd, const char *mode);

typedef struct gz_state {
	gzFile gz;
	int fd;
	FILE *fp;
	int err;
	z_stream strm;
	bool deflate;
} *gz_statep;

int nx_gzclose(gzFile file)
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

	if (state->deflate) {
		/* Finish the stream.  */
		stream->next_in = Z_NULL;
		stream->avail_in = 0;
		do {
			stream->next_out = next_out;
			stream->avail_out = MAX_LEN;

			ret = nx_deflate(stream, Z_FINISH);
			write(state->fd, next_out, MAX_LEN - stream->avail_out);
		} while (ret != Z_STREAM_END);

		ret = nx_deflateEnd(stream);
	} else
		ret = nx_inflateEnd(stream);

	if(state->fp == NULL)
		close(state->fd);
	else
		fclose(state->fp);

	free(next_out);
	free(state);

	return ret;
}

gzFile nx_gzdopen(int fd, const char *mode)
{
	return nx_gzopen_(NULL, fd, mode);
}

gzFile nx_gzopen(const char* path, const char *mode)
{
	return nx_gzopen_(path, 0, mode);
}

gzFile nx_gzopen_(const char* path, int fd, const char *mode)
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

		err = nx_deflateInit2(stream, level, Z_DEFLATED, 31,
					DEF_MEM_LEVEL, strategy);
		state->deflate = true;
	} else {
		err = nx_inflateInit(stream);
		state->deflate = false;
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

int nx_gzwrite (gzFile file, const void *buf, unsigned len)
{
	gz_statep state = (gz_statep) file;
	z_streamp stream = &(state->strm);
	int rc;
	uLong last_total_in = stream->total_in;
	unsigned char* next_out = malloc(sizeof(char)*len);

	if (next_out == NULL)
		return Z_MEM_ERROR;

	stream->next_in = (z_const Bytef *)buf;
	stream->avail_in = len;
	stream->next_out = next_out;
	stream->avail_out = len;

	rc = nx_deflate(stream, Z_NO_FLUSH);
	write(state->fd, next_out, len - stream->avail_out);

	free(next_out);
	if (rc != Z_OK && rc != Z_STREAM_END){
		state->err = rc;
		return 0;
	}
	return stream->total_in - last_total_in;
}
