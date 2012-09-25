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

unsigned  address_id  (address_t self); ///< \returns the id of a child at the current level of the tree.
address_t address_next(address_t self); ///< \returns the next child in that path through the tree or 0 if at a leaf.


#ifdef __cplusplus
} //extern "C"
#endif
