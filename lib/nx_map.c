/* nx_map.c -- implement map data structure
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

/** @file nx_map.c
 * \brief Implement a basic map data structure to be used throughout libnxz.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include "nx_map.h"
#include "nx_dbg.h"

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

#define MAP_INIT_SIZE 32

typedef struct map_entry {
	void *key;
	void *val;
} map_entry_t;

struct map {
	size_t size;
	size_t nb_entries;
	map_entry_t *array;
	pthread_rwlock_t rwlock;
};

static size_t get_index(void *key, size_t array_size)
{
	/* Use 64-bit FNV-1a hashing algorithm to calculate the index
	   https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function */
	uint64_t hash = FNV_OFFSET;

	for (int i = 0; i < sizeof(uintptr_t); i++) {
		uint64_t byte = (((uint64_t)key) >> i) & 0xffUL;
		hash ^= byte;
		hash *= FNV_PRIME;
	}
	return (size_t)(hash % array_size);
}

nx_map_t* nx_map_init(void)
{
	nx_map_t *m;

	m = malloc(sizeof(nx_map_t));
	if (!m) {
		prt_err("Failed to allocate map.\n");
		return NULL;
	}

	m->array = calloc(MAP_INIT_SIZE, sizeof(map_entry_t));
	if (!m->array) {
		prt_err("Failed to allocate internal array for map.\n");
		free(m);
		return NULL;
	}

	m->size = MAP_INIT_SIZE;
	m->nb_entries = 0;
	pthread_rwlock_init(&m->rwlock, NULL);

	return m;
}

int nx_map_destroy(nx_map_t *m) {
	if (m == NULL)
		return 0;

	m->size = m->nb_entries = 0;
	free(m->array);
	pthread_rwlock_destroy(&m->rwlock);
	free(m);

	return 0;
}

int nx_map_get(nx_map_t *m, void *key, void **val)
{
	size_t i;
	int ret = -1;

	if (m == NULL || m->array == NULL) {
		prt_err("Trying to access uninitialized map.\n");
		return -1;
	}

	*val = 0;
	i = get_index(key, m->size);

	pthread_rwlock_rdlock(&m->rwlock);

	for(int cnt = 0; cnt < m->size; cnt++) {
		if (m->array[i].key == key) {
			*val = m->array[i].val;
			ret = 0;
			break;
		}

		if (++i >= m->size)
			i = 0; // wrap around
	}

	pthread_rwlock_unlock(&m->rwlock);

	return ret;
}

static void insert_entry(nx_map_t *m, void *key, void *val)
{
	size_t i = get_index(key, m->size);

	for (int cnt = 0; cnt < m->size; cnt++) {
		if (m->array[i].key == NULL)
			break;

		if (++i >= m->size)
			i = 0; // wrap around
	}

	m->array[i].key = key;
	m->array[i].val = val;
	m->nb_entries++;
}

static void rehash_entries(nx_map_t *m, map_entry_t *old_array, size_t old_size)
{
	for (size_t i = 0; i < old_size; i++)
		if (old_array[i].key != NULL)
			insert_entry(m, old_array[i].key, old_array[i].val);
}

int nx_map_put(nx_map_t *m, void* key, void* val)
{
	if (m == NULL || m->array == NULL) {
		prt_err("Trying to alter uninitialized map.\n");
		return -1;
	}

	pthread_rwlock_wrlock(&m->rwlock);

	if (m->nb_entries >= m->size/2) {
		size_t old_size = m->size;
		map_entry_t *old_array = m->array;

		m->size *= 2;
		m->array = calloc(m->size, sizeof(map_entry_t));
		if (m->array == NULL) {
			prt_err("Failed to resize map\n");
			m->array = old_array;
			m->size = old_size;
			pthread_rwlock_unlock(&m->rwlock);
			return -1;
		}

		rehash_entries(m, old_array, old_size);
	}

	insert_entry(m, key, val);
	pthread_rwlock_unlock(&m->rwlock);

	return 0;
}

void* nx_map_remove(nx_map_t *m, void *key)
{
	void* val = NULL;

	if (m == NULL || m->array == NULL) {
		prt_err("Trying to access uninitialized map.\n");
		return NULL;
	}

	size_t i = get_index(key, m->size);

	pthread_rwlock_wrlock(&m->rwlock);

	for(int cnt = 0; cnt < m->size; cnt++) {
		if (m->array[i].key == key) {
			val = m->array[i].val;
			m->array[i].key = NULL;
			m->array[i].val = NULL;
			m->nb_entries--;
			break;
		}

		if (++i >= m->size)
			i = 0; // wrap around
	}

	pthread_rwlock_unlock(&m->rwlock);

	return val;
}
