#pragma once
#ifdef __cplusplus
extern "C"{
#endif

#include "metadata/metadata.h"

struct _tile_t
{ aabb_t aabb;  ///< bounding box for the tile
  ndio_t file;  ///< opened file for reading
  nd_t   shape;
  metadata_t meta; ///< handle to tile metadata.  Used to resolve filenames
  float* transform;
};

struct _tiles_t
{ tile_t *tiles;  ///< tiles array
  size_t  sz,     ///< tiles array length
          cap;    ///< tiles array capacity
  tilebase_cache_t cache; ///< used to cache tilebase information
//  char   *log;    ///< error log (NULL if no errors)
};


#ifdef __cplusplus
} //extern "C"
#endif

