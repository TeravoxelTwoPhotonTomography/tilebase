/**
 * \file
 * Recursively downsample a volume represented by a tile database.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "nd.h"
#include "tilebase.h"

/**
 * address_t describes a path through a tree.
 */
typedef struct _address_t *address_t;
typedef unsigned (*handler_t)(nd_t vol, address_t address);

unsigned render(tiles_t tiles, float x_um, float y_um, float z_um, size_t countof_leaf, handler_t onyield);

#ifdef __cplusplus
} //extern "C"
#endif
