/*
 * Copyright 2015, International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NXU_DBG_H_
#define _NXU_DBG_H_


#include <stdint.h>
#include <stdio.h>

extern FILE *nx_gzip_log;
extern int nx_gzip_trace;
extern unsigned int nx_gzip_inflate_impl;
extern unsigned int nx_gzip_deflate_impl;
extern unsigned int nx_gzip_inflate_flags;
extern unsigned int nx_gzip_deflate_flags;

extern int nx_dbg;

#define nx_gzip_trace_enabled()       (nx_gzip_trace & 0x1)
#define nx_gzip_hw_trace_enabled()    (nx_gzip_trace & 0x2)
#define nx_gzip_sw_trace_enabled()    (nx_gzip_trace & 0x4)
#define nx_gzip_gather_statistics()   (nx_gzip_trace & 0x8)

/* Use in case of an error */
#define prt_err(fmt, ...) do { if (nx_dbg >= 0)				\
		fprintf(nx_gzip_log, "%s:%u: Error: " fmt,		\
			__FILE__, __LINE__, ## __VA_ARGS__);		\
	} while (0)

/* Use in case of an warning */
#define prt_warn(fmt, ...) do {	if (nx_dbg >= 1)			\
		fprintf(nx_gzip_log, "%s:%u: Warning: " fmt,		\
			__FILE__, __LINE__, ## __VA_ARGS__);		\
	} while (0)

/* Informational printouts */
#define prt_info(fmt, ...) do {	if (nx_dbg >= 2)			\
		fprintf(nx_gzip_log, "Info: " fmt, ## __VA_ARGS__);	\
	} while (0)

/* Trace zlib wrapper code */
#define prt_trace(fmt, ...) do {					\
                if (nx_gzip_trace_enabled())				\
			fprintf(nx_gzip_log, "### " fmt, ## __VA_ARGS__); \
	} while (0)

/* Trace statistics */
#define prt_stat(fmt, ...) do {					\
                if (nx_gzip_gather_statistics())				\
			fprintf(nx_gzip_log, "### " fmt, ## __VA_ARGS__); \
	} while (0)

/* Trace zlib hardware implementation */
#define hw_trace(fmt, ...) do {						\
		if (nx_gzip_hw_trace_enabled())				\
			fprintf(nx_gzip_log, "hhh " fmt, ## __VA_ARGS__); \
	} while (0)

/* Trace zlib software implementation */
#define sw_trace(fmt, ...) do {						\
		if (nx_gzip_sw_trace_enabled())				\
			fprintf(nx_gzip_log, "sss " fmt, ## __VA_ARGS__); \
	} while (0)


/**
 * str_to_num - Convert string into number and copy with endings like
 *              KiB for kilobyte
 *              MiB for megabyte
 *              GiB for gigabyte
 */
uint64_t str_to_num(char *str);
void nx_lib_debug(int onoff);

#endif	/* _NXU_DBG_H_ */
