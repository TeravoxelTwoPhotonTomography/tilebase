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

tile_t  TileFromFile(const char* path, const char* format);
void    TileFree(tile_t self);
aabb_t  TileAABB(tile_t self);
ndio_t  TileFile(tile_t self);

tiles_t TileBaseOpen(const char *path);
void    TileBaseClose(tiles_t self);
//char*       TileBaseError();
//void        TileBaseResetError();

//int         TileBaseToSVG(tiles_t self,const char *path);
size_t  TileBaseCount(tiles_t self);

//loader_t    TileBaseCreateLoader(tiles_t self);

#ifdef __cplusplus
} //extern "C"
#endif