#pragma once
#ifdef __cplusplus
extern "C"{
#endif

typedef struct _tilebase_cache_t* tilebase_cache_t;
tilebase_cache_t TileBaseCacheOpen (const char *path, const char *mode);
void             TileBaseCacheClose(tilebase_cache_t self);
tilebase_cache_t TileBaseCacheRead (tilebase_cache_t self, tiles_t tiles);
tilebase_cache_t TileBaseCacheWrite(tilebase_cache_t self, const char* path, tile_t t);
char*            TileBaseCacheError(tilebase_cache_t self);
#ifdef __cplusplus
} //extern "C"
#endif