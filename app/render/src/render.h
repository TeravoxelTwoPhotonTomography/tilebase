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

struct render {
    double voxel_um[3],  ///< Desired voxel size of leaf nodes (x,y, and z).
           ori[3],       ///< Output box origin as a fraction of the total bounding box (0 to 1; x,y and z).
           size[3];      ///< Output box width as a fraction of the total bounding box (0 to 1; x, y and z).
    size_t countof_leaf; ///< Maximum size of a leaf node array (in elements).
    size_t nchildren;    ///< number of children per node
};



/**
 * address_t describes a path through a tree.
 */
// Requires: address.h - typedef struct _address_t *address_t;
typedef unsigned (*handler_t)(nd_t vol, address_t address, aabb_t bbox, void *args);
typedef nd_t     (*loader_t)(address_t address);

unsigned render       (const struct render *opts, tiles_t tiles, handler_t yield, void *args);
unsigned addresses    (const struct render *opts, tiles_t tiles, handler_t yield, void *args);
unsigned render_target(const struct render *opts, tiles_t tiles, handler_t yield, void *yield_args, loader_t loader, address_t target);

aabb_t AdjustTilesBoundingBox(tiles_t tiles, double o[3], double s[3]);
#ifdef __cplusplus
} //extern "C"
#endif
