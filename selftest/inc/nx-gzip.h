/*
 * Copyright 2016 IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Sogzipare Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _UAPI_MISC_VAS_H
#define _UAPI_MISC_VAS_H

#include <asm/ioctl.h>

#define VAS_FLAGS_PIN_WINDOW	0x1
#define VAS_FLAGS_HIGH_PRI	0x2

#define VAS_FTW_SETUP		_IOW('v', 1, struct vas_gzip_setup_attr)
#define VAS_842_TX_WIN_OPEN	_IOW('v', 2, struct vas_gzip_setup_attr)
#define VAS_GZIP_TX_WIN_OPEN	_IOW('v', 0x20, struct vas_gzip_setup_attr)

struct vas_gzip_setup_attr {
	int32_t		version;
	int16_t		vas_id;
	int16_t		reserved1;
	int64_t		flags;
	int64_t		reserved2[6];
};

#endif /* _UAPI_MISC_VAS_H */
