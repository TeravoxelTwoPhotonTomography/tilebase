/**
 * \file
 * Recursively downsample a volume represented by a tile database.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "nd.h"
#include "tilebase.h"
#include "address.h"
/**
 * address_t describes a path through a tree.
 */
// Requires: address.h - typedef struct _address_t *address_t;
typedef unsigned (*handler_t)(nd_t vol, address_t address, void *args);
typedef nd_t     (*loader_t)(address_t address);

unsigned render       (tiles_t tiles, float voxel_um[3], float ori[3], float size[3], size_t nchildren, size_t countof_leaf, handler_t yield, void *args);
unsigned addresses    (tiles_t tiles, float voxel_um[3], float ori[3], float size[3], size_t nchildren, size_t countof_leaf, handler_t yield, void *args);
unsigned render_target(tiles_t tiles, float voxel_um[3], float ori[3], float size[3], size_t nchildren, size_t countof_leaf, handler_t yield, void *yield_args, loader_t loader, address_t target);

#ifdef __cplusplus
} //extern "C"
#endif
