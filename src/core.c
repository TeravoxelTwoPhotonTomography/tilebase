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
#include "metadata.h"
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include "util/dirent.win.h"
#else
#include <dirent.h>
#endif

/// @cond DEFINES
#define ENDL               "\n"
#define LOG(...)           fprintf(stderr,__VA_ARGS__)
#define TRY(e)             do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define NEW(type,e,nelem)  TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define ZERO(type,e,nelem) memset((e),0,sizeof(type)*nelem)
#define SAFEFREE(e)        if(e){free(e); (e)=NULL;} 
#define countof(e)         (sizeof(e)/sizeof(*(e)))
/// @endcond

struct _tile_t
{ aabb_t aabb;  ///< bounding box for the tile
  ndio_t file;  ///< opened file for reading
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
  NEW(struct _tile_t,out,1);
  ZERO(struct _tile_t,out,1);
  TRY(meta=MetadataOpen(path,format,"r"));
  TRY(MetadataGetTileAABB(meta,out));
  TRY(out->file=MetadataOpenVolume(meta,"r"));
  MetadataClose(meta);
  return out;
Error:
  MetadataClose(meta);
  TileFree(out);
  return 0;
}

void TileFree(tile_t self)
{ if(!self) return;
  AABBFree(self->aabb);
  ndioClose(self->file);
  free(self);
}

aabb_t TileAABB(tile_t self) { return self->aabb; }
ndio_t TileFile(tile_t self) { return self->file; }

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
 * \param[in,out]   out   Must have sufficient length (as indicated by \a n).
 * \returns \a out on success, otherwise 0.
 */
static char* join(char* out, size_t n, const char *path1, const char* path2)
{ size_t n1,n2;
  TRY(n<(n1=strlen(path1))+(n2=strlen(path2))+2); // +1 for the /, +1 for the terminating null
  strcat(out,path1);
  strcat(out+n1,"/");
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
static unsigned addtiles(tiles_t tiles,const char *path)
{ int any=0;
  char next[1024];
  DIR *dir=0;
  struct dirent *ent;
  TRY(path);
  TRY(dir=opendir(path));
  while(ent=readdir(dir)) // need to avoid '.' and '..'?
  { if(ent->d_type==DT_DIR)
      TRY(addtiles(tiles,join(next,sizeof(next),path,ent->d_name)));
    else
      if(ent->d_type==DT_REG) any=1;
  }
  closedir(dir);
  if(any)
    TRY(push(tiles,TileFromFile(path,NULL))); // autodetect tile format
  return 1;
Error:
  if(dir) closedir(dir);
  return 0; 
}

/**
 * Open all the tiles contained in a directory tree rooted at \a path.
 */
tiles_t TileBaseOpen(const char *path)
{ tiles_t out=0;
  NEW( struct _tiles_t,out,1);
  ZERO(struct _tiles_t,out,1);
  TRY(addtiles(out,path));
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