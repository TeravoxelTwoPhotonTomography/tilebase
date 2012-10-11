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

tiles_t TileBaseOpen(const char *path, const char* format);
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

//loader_t    TileBaseCreateLoader(tiles_t self);

#ifdef __cplusplus
} //extern "C"
#endif