#pragma once
#ifdef __cplusplus
extern "C"{
#endif

#include "tilebase.h"

typedef struct _tilebase_cache_t* tilebase_cache_t;
tilebase_cache_t TileBaseCacheOpen (const char *path, const char *mode);
tilebase_cache_t TileBaseCacheOpenWithRoot(const char *filename, const char *mode, char* root);
void             TileBaseCacheClose(tilebase_cache_t self);
tilebase_cache_t TileBaseCacheRead (tilebase_cache_t self, tiles_t *tiles);
tilebase_cache_t TileBaseCacheWrite(tilebase_cache_t self, const char* path, tile_t t);
tilebase_cache_t TileBaseCacheWriteMany(tilebase_cache_t self, tile_t *t, size_t ntiles);
char*            TileBaseCacheError(tilebase_cache_t self);
#ifdef __cplusplus
} //extern "C"
#endif

