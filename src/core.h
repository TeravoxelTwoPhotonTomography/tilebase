/** \file
 *  Tile database.  Core functionality.
 *
 *  The header requires several others be included before it.
 *  The recommended pattern is:
 *  \code{c}
 *  #include <stdlib.h>
 *  #include "nd.h"
 *  #include "aabb.h"
 *  #include "core.h"
 *  \endcode
 *  \todo Use the standard pattern in the master header.
 *
 *  \todo [design]  TileBaseOpen* starts trying to read/build the cache.
 *        When it should probably do nothing but the minimal amount of work to
 *        make a valid object.  Then, when we want to associate different data/
 *        configure the thing we can do that with the API and not a seperate
 *        constructor (see the *OpenWithProgressIndicator constructor).  Not 
 *        a critical problem as is, but if I wanted to add other callbacks etc.. 
 * 
 *  \author Nathan Clack
 *  \date   July 2012
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

//typedef struct _ndio_t *ndio_t; // Requires ndio.h
//typedef struct _aabb_t *aabb_t; // Requires aabb.h
typedef struct _tile_t  *tile_t;
typedef struct _tiles_t *tiles_t;

typedef void (*tilebase_progress_t)(const char* path, void* data);

tiles_t TileBaseOpen(const char *path, const char* format);
tiles_t TileBaseOpenWithProgressIndicator(const char *path, const char* format,
                                          tilebase_progress_t callback, void* cbdata);
void    TileBaseClose(tiles_t self);
//char*       TileBaseError();
//void        TileBaseResetError();

//int         TileBaseToSVG(tiles_t self,const char *path);
size_t  TileBaseCount(tiles_t self);
tile_t* TileBaseArray(tiles_t self);
aabb_t  TileBaseAABB(tiles_t self);
float   TileBaseVoxelSize(tiles_t self, unsigned idim);

tile_t  TileNew(const char* path,const char* metadata_format);
void    TileFree(tile_t tile);
aabb_t  TileAABB(tile_t self); // returned AABB owned by tile.
ndio_t  TileFile(tile_t self); // returned file handle already opened.  Owned by tile.
nd_t    TileShape(tile_t self);// returned array is still owned by the tile.
float*  TileTransform(tile_t self);
float   TileVoxelSize(tile_t self, unsigned idim);
const char* TilePath(tile_t self); // returned string is owned by the tile.

//loader_t    TileBaseCreateLoader(tiles_t self);

#ifdef __cplusplus
} //extern "C"
#endif