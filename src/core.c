/** \file
 *  Tile database.  Core functionality.
 *
 *  Tile's are scalar volumes, such as 3d intensity data, with some physical bounds.
 *
 *  The tile database is a collection of tiles loaded from disk.  Each tile gets its
 *  own directory.  That directory contains the tile's intensity data and data used
 *  to determine the physical location and shape of the tile.
 *
 *  The tile directories may be organized in an arbitrary directory hierarchy
 *  descendant from a common root directory.
 *
 *  \todo Logging
 * 
 *  \author Nathan Clack
 *  \date   July 2012
 */
#include <stdlib.h>
#include "nd.h"
#include "aabb.h"
#include "core.h"
#include "metadata/metadata.h"
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include "util/dirent.win.h"
#define PATHSEP '\\'
#else
#include <dirent.h>
#define PATHSEP '/'
#endif

/// @cond DEFINES
#define ENDL               "\n"
#define LOG(...)           fprintf(stderr,__VA_ARGS__)
#define TRY(e)             do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto Error;}} while(0)
#define NEW(type,e,nelem)  TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define ZERO(type,e,nelem) memset((e),0,sizeof(type)*nelem)
#define SAFEFREE(e)        if(e){free(e); (e)=NULL;} 
#define countof(e)         (sizeof(e)/sizeof(*(e)))

static void breakme() {};
/// @endcond

struct _tile_t
{ aabb_t aabb;  ///< bounding box for the tile
  ndio_t file;  ///< opened file for reading
  nd_t   shape;
  float* transform;
};

struct _tiles_t
{ tile_t *tiles;  ///< tiles array
  size_t  sz,     ///< tiles array length
          cap;    ///< tiles array capacity
//  char   *log;    ///< error log (NULL if no errors)
};

//
// === TILE API ===
//

void TileFree(tile_t self)
{ if(!self) return;
  AABBFree(self->aabb);
  ndioClose(self->file);
  ndfree(self->shape);
  if(self->transform) free(self->transform);
  free(self);
}

/**
 * Load tile data from the tile at \a path using the metadata format \a format.
 * Makes a new tile; the caller is responsible for cleaning up with TileFree() 
 * when finished with the reference.
 * 
 * \param[in]   path    Path to the tile.  A NULL terminated string.
 * \param[in]   format  Metadata format used to read tile.  If NULL, the format
 *                      will be auto-detected.
 */
tile_t TileFromFile(const char* path, const char* format)
{ tile_t out=0;
  metadata_t meta=0;
  unsigned n;
  NEW(struct _tile_t,out,1);
  ZERO(struct _tile_t,out,1);
  TRY(meta=MetadataOpen(path,format,"r"));
  TRY(out->aabb=AABBMake(0)); 
  TRY(MetadataGetTileAABB(meta,out));
  TRY(out->file=MetadataOpenVolume(meta,"r"));
  TRY(out->shape=ndioShape(out->file));
  n=ndndim(out->shape);
  NEW(float,out->transform,(n+1)*(n+1));
  TRY(MetadataGetTransform(meta,out->transform));
  MetadataClose(meta);
  LOG("TileBase: Loaded %s"ENDL,path);
  return out;
Error:
  MetadataClose(meta);
  TileFree(out);
  return 0;
}


aabb_t TileAABB(tile_t self) { return self->aabb; }
ndio_t TileFile(tile_t self) { return self->file; }

/**
 * Computes the voxel size from the tile database for dimension \a idim.
 * Use AABBNDim(TileAABB(self)) to get the number of dimensions.
 *
 * \returns The voxel size in physical units (e.g. nanometers).  The actual
 *          length scale depends on the units used for the aabb_t.
 */
float TileVoxelSize(tile_t self, unsigned idim)
{ int64_t *shape;
  float r;
  TRY(self);
  TRY(AABBGet(self->aabb,NULL,NULL,&shape));
  r=shape[idim]/(float)ndshape(self->shape)[idim];
  return r;
Error:
  return 0.0;
}


/**
 * Get pixel to space transform.
 * For example, a point at index ir=(ix,iy,iz) will be mapped to r=T.ir,
 * where T is the \a matrix computed by this function.
 *
 * \returns an array of (ndim+1)*(ndim+1) elements where ndim is the 
 *          dimensionality of the volume read from TileFile().
 *                        
 */
float* TileTransform(tile_t self)
{ return self->transform;
}

//
//  === TILE COLLECTION ===
//

static int maybe_resize(tiles_t self,size_t nelem)
{ if(nelem>self->cap)
    self->cap=self->cap*1.2+50;
  TRY(self->tiles=realloc(self->tiles,self->cap*sizeof(tile_t)));
  return 1;
Error:
  return 0;
}

static int push(tiles_t self,tile_t tile)
{ if(!tile) return 1;
  TRY(maybe_resize(self,self->sz+1));
  self->tiles[self->sz++]=tile;
  return 1;
Error: 
  return 0;
}

/**
 * Concatenates two paths stings adding a path seperator.
 * \param[in,out]   out   Must have length of at least \a n.
 * \param[in]       n     Must be greater than the length of path1+path2+2.
 * \returns \a out on success, otherwise 0.
 */
static char* join(char* out, size_t n, const char *path1, const char* path2)
{ size_t n1,n2;
  TRY(n>(n1=strlen(path1))+(n2=strlen(path2))+2); // +1 for the /, +1 for the terminating null
  memset(out,0,n);
  strcat(out,path1);
  out[n1]=PATHSEP;
  strcat(out+n1+1,path2);
  return out;
Error:
  return 0;

}

/**
 * Recursively descend path looking for tiles.
 *
 * If \a path contains entries that are not directories,
 * also try to open \a path as a tile.  If a tile is
 * successfully loaded, it's added to the internal tile list.
 */
static unsigned addtiles(tiles_t tiles,const char *path, const char* format)
{ int any=0;
  char next[1024]={0};
  DIR *dir=0;
  struct dirent *ent;
  TRY(path);
  TRY(dir=opendir(path));
  while((ent=readdir(dir)))
  { if(ent->d_type==DT_DIR)
    { if(ent->d_name[0]!='.') //ignore "dot" hidden files and directories (including '.' and '..')
        if(!addtiles(tiles,join(next,sizeof(next),path,ent->d_name),format)) // maybe add subdirs -- some subdirs might not be valid
          continue; 
    }
    else if(ent->d_type==DT_REG) any=1;
  }
  closedir(dir);
  if(any)
    TRY(push(tiles,TileFromFile(path,format)));
  return 1;
Error:
  if(dir) closedir(dir);
  return 0; 
}

/**
 * Open all the tiles contained in a directory tree rooted at \a path.
 */
tiles_t TileBaseOpen(const char* path, const char* format)
{ tiles_t out=0;
  NEW( struct _tiles_t,out,1);
  ZERO(struct _tiles_t,out,1);
  TRY(addtiles(out,path,format));
  return out;
Error:
  TileBaseClose(out);
  return 0;
}

/**
 * Release resources.
 */
void TileBaseClose(tiles_t self)
{ if(!self) return;
  if(self->tiles)
  { size_t i;
    for(i=0;i<self->sz;++i)
      TileFree(self->tiles[i]);
    free(self->tiles);
  }
}

/**
 * The number of tiles.
 */
size_t TileBaseCount(tiles_t self)
{ return self?self->sz:0;
}

/**
 * The tile array.
 */
tile_t* TileBaseArray(tiles_t self)
{ return self?self->tiles:0;
}

/**
 * Computes the box bounding the tiles.
 * \returns 0 if error, otherwise the bounding box of the entire tile set.
 *          The caller is responsible for freeing the returned object with AABBFree().
 */
aabb_t TileBaseAABB(tiles_t self)
{ aabb_t out=0;
  size_t i;
  for(i=0;i<self->sz;++i)
    out=AABBUnionIP(out,self->tiles[i]->aabb);
  return out;
Error:
  return 0;
}

/**
 * Computes the voxel size from the tile database for dimension \a idim.
 * Use AABBNDim(TileAABB(TileBaseArray(self))) to get the number of dimensions.
 *
 * \returns The voxel size in physical units (e.g. nanometers).  The actual
 *          length scale depends on the units used for the aabb_t.
 */
float TileBaseVoxelSize(tiles_t self, unsigned idim)
{ TRY(self && self->sz>0);  
  return TileVoxelSize(self->tiles[0],idim);// assumes all tiles have the same shape
Error:
  return 0.0;
}