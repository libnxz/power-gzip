#ifndef NX_MAP_H
#define NX_MAP_H

#include <stdint.h>
#include <stdlib.h>

typedef struct map nx_map_t;

nx_map_t* nx_map_init(void);

int nx_map_destroy(nx_map_t *h);

int nx_map_get(nx_map_t *h, void *key, void **val);

int nx_map_put(nx_map_t *h, void *key, void *val);

void *nx_map_remove(nx_map_t *h, void *key);


#endif
