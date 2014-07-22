#pragma once
#ifdef __cplusplus
extern "C"{
#endif

//Requires: #include "metadata/metadata.h" before this file is included.

struct _tile_t
{ aabb_t aabb;  ///< bounding box for the tile
  ndio_t file;  ///< opened file for reading
  nd_t   shape;
  nd_t   crop;
  metadata_t meta; ///< handle to tile metadata.  Used to resolve filenames  
  float* transform;
  char   path[1024];
  char   metadata_format[64];
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

