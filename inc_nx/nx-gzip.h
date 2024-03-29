/*
 * Copyright (C) IBM Corporation, 2016-2020
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
#ifndef _NX_GZIP_H
#define _NX_GZIP_H

#include <asm/ioctl.h>

/* NX function codes */
#define NX_FUNC_COMP_GZIP   2

#define VAS_FLAGS_PIN_WINDOW	0x1
#define VAS_FLAGS_HIGH_PRI	0x2

#define VAS_FTW_SETUP		_IOW('v', 1, struct vas_gzip_setup_attr)
#define VAS_842_TX_WIN_OPEN	_IOW('v', 2, struct vas_gzip_setup_attr)
#define VAS_GZIP_TX_WIN_OPEN	_IOW('v', 0x20, struct vas_gzip_setup_attr)

struct vas_gzip_setup_attr {
	uint32_t	version;
	int16_t		vas_id;
	uint16_t	reserved1;
	uint64_t	flags;
	uint64_t	reserved2[6];
};

#endif /* _NX_GZIP_H */
