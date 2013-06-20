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
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include "nd.h"
#include "aabb.h"
#include "core.h"
#include "metadata/metadata.h"
#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "core.priv.h" // defines tile_t and tiles_t structs

#include <limits.h> // for PATH_MAX (for realpath)
#include <stdlib.h> // for realpath()

#ifdef _MSC_VER
#include "util/dirent.win.h"
#define PATHSEP '\\'
#undef  min
#else
#include <dirent.h>
#define PATHSEP '/'
#endif

#define min(a,b) (a<b?a:b)

/// @cond DEFINES
#define ENDL               "\n"
#define LOG(...)           fprintf(stderr,__VA_ARGS__)
#define TRY(e)             do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto Error;}} while(0)
#define TRYLBL(e,lbl)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto lbl;}} while(0)
#define NEW(type,e,nelem)  TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define ZERO(type,e,nelem) memset((e),0,sizeof(type)*nelem)
#define SAFEFREE(e)        if(e){free(e); (e)=NULL;} 
#define countof(e)         (sizeof(e)/sizeof(*(e)))

static void breakme() {};
/// @endcond

#ifdef _MSC_VER
#include <windows.h>
#define PATH_MAX (MAX_PATH)
/** Stand-in for realpath() for windows that uses GetFullPathName().
 *  Only conforms to how realpath is used here.  Does not conform to POSIX.
 */
static char *realpath(const char* path,char *out)
{ int n=0;
  TRY(n=GetFullPathName(path,out?MAX_PATH:0,out,NULL));
  if(!out)
  { NEW(char,out,n);
    ZERO(char,out,n);
    TRY(GetFullPathName(path,n,out,NULL));
  }
  return out;
Error:
  return 0;
}
#endif

static unsigned ispathsep(char c)
{ switch(c)
  {
#ifdef _MSC_VER
    case '\\':
#endif
    case '/': return 1;
  default:
    return 0;
  }
}

//
// === TILE API ===
//

void TileFree(tile_t self)
{ if(!self) return;
  AABBFree(self->aabb);
  ndioClose(self->file);
  ndfree(self->shape);
  if(self->transform) free(self->transform);
  MetadataClose(self->meta);
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
tile_t TileNew(const char* path, const char* metadata_format)
{ tile_t out=0;
  metadata_t meta=0;
  NEW(struct _tile_t,out,1);
  ZERO(struct _tile_t,out,1);
  strncpy(out->path,path,sizeof(out->path));
  TRY(out->path[sizeof(out->path)-1]=='\0');
  if(metadata_format)
  { strncpy(out->metadata_format,metadata_format,sizeof(out->metadata_format));
    TRY(out->metadata_format[sizeof(out->metadata_format)-1]=='\0');
  }
  return out;
Error:
  TileFree(out);
  return 0;
}

static metadata_t TileMetadata(tile_t self)
{ if(!self->meta)
    TRY(self->meta=MetadataOpen(self->path,self->metadata_format,"r"));
  return self->meta;
Error:
  return 0;
}
aabb_t TileAABB(tile_t self)
{ if(!self->aabb)
  { TRY(self->aabb=AABBMake(0));
    TRY(MetadataGetTileAABB(TileMetadata(self),self));
  }
  return self->aabb;
Error:
  AABBFree(self->aabb);
  self->aabb=0;
  return 0;
}
ndio_t TileFile(tile_t self) 
{ if(!self->file)
    TRY(self->file=MetadataOpenVolume(TileMetadata(self),"r"));
  return self->file; 
Error:
  return 0;
}
nd_t TileShape(tile_t self)
{ if(!self->shape)
    TRY(self->shape=ndioShape(TileFile(self)));
  return self->shape;
Error:
  return 0;
}

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
  nd_t v;
  TRY(self);
  TRY(v=TileShape(self));
  TRY(AABBGet(self->aabb,NULL,NULL,&shape));
  r=shape[idim]/(float)ndshape(v)[idim];
  return r;
Error:
  return 0.0;
}

/** Gets the tile path.
    \returns a const string with the tile's path.
 */
const char* TilePath(tile_t self)
{ return self->path;
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
{ if(!self->transform)
  { unsigned n;
    TRY(n=ndndim(TileShape(self)));
    NEW(float,self->transform,(n+1)*(n+1));
    TRY(MetadataGetTransform(TileMetadata(self),self->transform));
  }
  return self->transform;
Error:
  return 0;
}

//
//  === TILE COLLECTION ===
//

static int maybe_resize(tiles_t self,size_t nelem)
{ if(nelem>self->cap)
    self->cap=(size_t)(nelem*1.2+50);
  TRY(self->tiles=(tile_t*)realloc(self->tiles,self->cap*sizeof(tile_t)));
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

static int pop(tiles_t self)
{ if(self->sz>0)
  { TileFree(self->tiles[--self->sz]);
    self->tiles[self->sz]=0;
  }
  return self->sz>0;
}

static int push_many(tiles_t self,tiles_t other)
{ size_t i=0;
  if(!other) return 1;
  TRY(maybe_resize(self,self->sz+other->sz));
  memcpy(self->tiles+self->sz,other->tiles,other->sz*sizeof(*other->tiles));
  self->sz+=other->sz;
  other->sz=0; // effectively transfer the contents to `self`.  `other` must still be closed.
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

/** dirent.d_type is not as cross platform as all that.
    Uses stat instead.
*/
#include <sys/stat.h>
static int isdir(const char *path, struct dirent *ent, int *isok)
{ char full[PATH_MAX]={0};  
  struct stat s;
  strcat(full,path);
  strcat(full,"/");
  strcat(full,ent->d_name);
  if(isok) *isok=0;
  TRY(0==stat(full,&s));
  if(isok) *isok=1;
  return S_ISDIR(s.st_mode);
Error:
  perror("isdir()");
  return 0;  
}

/**
 * Recursively descend path looking for tiles.
 *
 * Data is expected to be in the leaves.  A leaf is a subdirectory containing 
 * no directories.
 */
static unsigned addtiles(tiles_t tiles,const char *path, const char* format, tilebase_progress_t callback, void *cbdata)
{ int any=0;
  char next[1024]={0};
  DIR *dir=0;
  struct dirent *ent;
  TRY(path);

  // First, try to open a cache at path
  { tiles_t local=0;
    TileBaseCacheClose(TileBaseCacheRead(TileBaseCacheOpen(path,"r"),&local));
    if(local)
    { size_t i;
      for(i=0;i<local->sz;++i)
      { if(callback) callback(local->tiles[i]->path,cbdata);
        TileBaseCacheWrite(tiles->cache,local->tiles[i]->path,local->tiles[i]);
      }
      push_many(tiles,local);
      TileBaseClose(local);
      return 1;
    }
  }

  // No cache, process the directory
  TRY(dir=opendir(path));
  while((ent=readdir(dir)))
  { int isok=1;
    if(ent->d_name[0]!='.') //ignore "dot" hidden files and directories (including '.' and '..')
    { if(isdir(path,ent,&isok)) 
      { any=1; // has a subdirectory ==> not a leaf
        if(!addtiles(tiles,join(next,sizeof(next),path,ent->d_name),format,callback,cbdata)) // maybe add subdirs -- some subdirs might not be valid
          continue;
      }
    }
    TRY(isok);
  }
  
  if(!any) // then it's a leaf
  { tile_t t=0;
    TRY(push(tiles,t=TileNew(path,format)));
    if(callback) callback(path,cbdata);
    if(t) // !!! insufficient?...Tile is lazy, so we don't know it's valid at constuction
      TRYLBL(TileBaseCacheWrite(tiles->cache,path,t),CacheWriteError); // this should be able to handle bad tiles by silently failing.
  }
  return 1;
CacheWriteError:
  pop(tiles);
Error:
  if(dir) closedir(dir);
  return 0; 
}

/**
 * Open all the tiles contained in a directory tree rooted at \a path.
 * \param[in] path   The root patht ot the directory tree containing all the tiles.
 * \param[in] format The metadata format for the tiles.  May be the empty string or NULL,
 *                   in which case the metadata format will be guessed.
 */
tiles_t TileBaseOpen(const char* path, const char* format)
{ return TileBaseOpenWithProgressIndicator(path, format, 0, 0);
}

/**
 * Just like TileBaseOpen() but calls callback(path,cbdata) when a tile at 
 * \a path is added.
 * Open all the tiles contained in a directory tree rooted at \a path.
 * \param[in] path     The root patht ot the directory tree containing all the tiles.
 * \param[in] format   The metadata format for the tiles.  May be the empty string or NULL,
 *                     in which case the metadata format will be guessed.
 * \param[in] callback The progress indicator callback.
 * \param[in] cbdata   A pointer that will be passed to the callback function.
 *                     Otherwise this is left alone.  Use this to pass state
 *                     to/from the callback 
 */
tiles_t TileBaseOpenWithProgressIndicator(const char *path_, const char* format, tilebase_progress_t callback, void *cbdata)
{ tiles_t out=0;
  tilebase_cache_t cache=0;
  char path[PATH_MAX+1]={0};
  TRY(realpath(path_,path));// canonicalize input path
  if((cache=TileBaseCacheOpen(path,"r")) && TileBaseCacheRead(cache,&out) && out && out->sz>0)
  { TileBaseCacheClose(cache);
  } else
  { if(!cache) // no cache was found so try to make one from scratch
    { NEW( struct _tiles_t,out,1);
      ZERO(struct _tiles_t,out,1);
      out->cache=TileBaseCacheOpen(path,"w");
      TRY(addtiles(out,path,format,callback,cbdata));
      TileBaseCacheClose(out->cache);
    }
    else
      LOG("Error reading cache file at:\n\t%s\n\n\t%s\n",path,TileBaseCacheError(cache));
  }
  return out;
Error:
  TileBaseClose(out);
  return 0;
}

/** 
 * Free a contiguous array of tiles.
 */
void TileFreeArray(tile_t *tiles,size_t sz)
{ if(tiles)
  { size_t i;
    for(i=0;i<sz;++i)
      TileFree(tiles[i]);
    free(tiles);
  }
}

/**
 * Release resources.
 */
void TileBaseClose(tiles_t self)
{ if(!self) return;
  TileFreeArray(self->tiles,self->sz);  
  free(self);
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
    out=AABBUnionIP(out,TileAABB(self->tiles[i]));
  return out;
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

/** \returns NULL on failure, otherwise 
             the prefix string common to all tiles in \a tiles.
             The caller must free the returned string.
    \param[in] tiles  The array of tiles to query.
    \param[in] ntiles The number of elements in the \a tiles array.
 */
char* TilesCommonRoot(const tile_t* tiles, size_t ntiles)
{ size_t n,i,j,s; // s marks the last path sep
  const char *ref=0;
  TRY(ntiles>0);
  n=strlen(ref=TilePath(tiles[0]))+1; // n is first diff, so one past last
  for(i=1;i<ntiles;++i)
  { const char *b=TilePath(tiles[i]);
    n=min(n,strlen(b)+1);
    for(j=0;j<n && (b[j]==ref[j]);j++)
      if(ispathsep(b[j]))
        s=j;
    n=min(j,s+1);
  }
  { char *out=0;
    NEW(char,out,n+1);
    memcpy(out,ref,n);
    out[n]='\0';
    return out;
  }
Error:
  return 0;
}

/**
 * \returns NULL on failure, otherwise an array that the caller is responsible
 *          for freeing.  The returned array contains \a *nout tiles where
 *          the predicate defined by \a test evaluated to true.
 * \param[in]     in    The input array.
 * \param[in]     n     The number of tiles in the input array.
 * \param[out]    nout  The number of tiles in the output array.
 * \param[in]     test  The predicate, called once for each tile.
 * \param[in,out] ctx   This gets directly passed to test.
 */
tile_t *TilesFilter(tile_t *in, size_t n, size_t *nout, unsigned (*test)(tile_t *a,void *ctx), void *ctx)
{ size_t i,c=0;
  tile_t *out=0;
  NEW(tile_t,out,n);
  ZERO(tile_t,out,n);
  for(i=0;i<n;++i)
    if(test(in+i,ctx))
      out[c++]=in[i];
  if(nout) *nout=c;
  return out;
Error:
  if(out) free(out);
  return 0;
}