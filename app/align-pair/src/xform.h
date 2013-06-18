#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "tilebase.h"

struct nd_subarray_t
{ nd_t     shape; // must be free'd with ndfree()
  size_t  *ori;   // owned by the tile used in construction, do not free.
};

aabb_t AABBToPx(tile_t self, aabb_t box_nm); ///< caller must free returned value with AABBFree

struct nd_subarray_t AABBToSubarray(tile_t self, aabb_t box_nm); ///< caller must use ndfree to release shape field in returned struct.

#ifdef __cplusplus
} //extern "C"
#endif