/**
 * \file
 * Recursively downsample a volume represented by a tile database.
 */
#include <stdio.h>
#include <string.h>
#include "render.h"
#include "filter.h"
#include "xform.h"
#include "address.h"
#include <math.h> //for sqrt

#include "tictoc.h" // for profiling

#define countof(e) (sizeof(e)/sizeof(*(e)))

#define ENDL        "\n"
#define LOG(...)    fprintf(stderr,__VA_ARGS__) 
#define TRY(e)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto Error;}} while(0)
#define NEW(T,e,N)  TRY((e)=malloc(sizeof(T)*(N)))
#define ZERO(T,e,N) memset((e),0,(N)*sizeof(T))

#define REALLOC(T,e,N)  TRY((e)=realloc((e),sizeof(T)*(N)))

//#define DEBUG
//#define DEBUG_DUMP_IMAGES
#ifdef DEBUG
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif


// === ADDRESSING A PATH IN THE TREE ===

static void breakme() {LOG(ENDL);}


// === RENDERING ===

typedef struct _filter_workspace
{ nd_t filters[3];  ///< filter for each axis
  nd_t gpu[2];      ///< double buffer on gpu for serial seperable convolutions
  unsigned capacity;///< capacity of alloc'd gpu buffers
  unsigned i;       ///< current gpu buffer
  nd_conv_params_t params;
} filter_workspace;

typedef struct _affine_workspace
{ nd_t host_xform,gpu_xform;
  nd_affine_params_t params;
} affine_workspace;

/// Common arguments and memory context used for building the tree
typedef struct _desc_t
{ tiles_t tiles;
  float x_nm,y_nm,z_nm,voxvol_nm3;
  size_t countof_leaf;
  handler_t yield;

  nd_t ref;
  nd_t bufs[8]; //2*4 bufs - octree would be 2*8
  int  inuse[8];
  filter_workspace fws;
  affine_workspace aws;
  float *transform;
} desc_t;

static void filter_workspace__init(filter_workspace *ws)
{ memset(ws,0,sizeof(*ws));
  ws->params.boundary_condition=nd_boundary_replicate;
}

static void affine_workspace__init(affine_workspace *ws)
{ memset(ws,0,sizeof(*ws));
  ws->params.boundary_value=0x8000;
};

static desc_t make_desc(tiles_t tiles, float x_um, float y_um, float z_um, size_t countof_leaf, handler_t yield)
{ const float um2nm=1e3;

  desc_t out;
  memset(&out,0,sizeof(out));
  out.tiles=tiles;
  out.x_nm=x_um*um2nm;
  out.y_nm=y_um*um2nm;
  out.z_nm=z_um*um2nm;
  out.voxvol_nm3=out.x_nm*out.y_nm*out.z_nm;
  out.countof_leaf=countof_leaf;
  out.yield=yield;
  filter_workspace__init(&out.fws);
  affine_workspace__init(&out.aws);

  return out;
}

static void cleanup_desc(desc_t *desc)
{ size_t i;
  ndfree(desc->ref);
  for(i=0;i<countof(desc->bufs);++i)
    ndfree(desc->bufs[i]);
  if(desc->transform) free(desc->transform);
}

static desc_t* set_ref_shape(desc_t *desc, nd_t v)
{ nd_t t;
  if(desc->ref) // init first time only
    return desc;
  TRY(ndreshape(ndcast(desc->ref=ndinit(),ndtype(v)),ndndim(v),ndshape(v)));
  { int i;
    TRY(ndShapeSet(ndcast(t=ndinit(),ndtype(desc->ref)),0,desc->countof_leaf));
    for(i=0;i<countof(desc->bufs);++i)
      TRY(desc->bufs[i]=ndcuda(t,0));
  }
  return desc;
Error:
  LOG("\t[nd Error]:"ENDL "\t%s"ENDL,nderror(t));
  return 0;
}

static unsigned isleaf(const desc_t*const desc, aabb_t bbox)
{ int64_t c=AABBVolume(bbox)/(int64_t)desc->voxvol_nm3;
  return c<(int64_t)desc->countof_leaf;
}

static nd_t alloc_vol(desc_t *desc, aabb_t bbox, int64_t x_nm, int64_t y_nm, int64_t z_nm)
{ nd_t v;
  int64_t *shape_nm;
  int64_t  res[]={x_nm,y_nm,z_nm};
  size_t ndim,i;
  for(i=0;i<countof(desc->bufs) && desc->inuse[i];++i); // search for first unused
  TRY(i<countof(desc->bufs));                           // check that one was available  
  TRY(v=desc->bufs[i]);                                 // ensure bufs was init'd - see set_ref_shape()
  desc->inuse[i]=1;
  DBG("-- alloc_vol(): buf=%d"ENDL,i);

  AABBGet(bbox,&ndim,0,&shape_nm);
  for(i=0;i<ndim;++i) // set spatial dimensions
    TRY(ndShapeSet(v,i,shape_nm[i]/res[i]));
  for(;i<ndndim(desc->ref);++i) // other dimensions are same as input
    TRY(ndShapeSet(v,(unsigned)i,ndshape(desc->ref)[i]));
  TRY(ndCudaSyncShape(v));
  TRY(ndfill(v,desc->aws.params.boundary_value));  
  return v;
Error:
  return 0;
}

static unsigned release_vol(desc_t *desc,nd_t a)
{ size_t i;
  for(i=0;i<countof(desc->bufs) && desc->bufs[i]!=a;++i); // search for buffer corresponding to a
  TRY(i<countof(desc->bufs));                             // ensure one was found
  TRY(desc->inuse[i]);                                    // ensure it was marked inuse. (sanity check)
  desc->inuse[i]=0;
  DBG("-- release_vol(): buf=%d"ENDL,i);
  return 1;
Error:
  return 0;
}

static unsigned filter_workspace__gpu_resize(filter_workspace *ws, nd_t vol)
{ if(!ws->gpu[0])
  { TRY(ws->gpu[0]=ndcuda(vol,0));
    TRY(ws->gpu[1]=ndcuda(vol,0));
    ws->capacity=(unsigned)ndnbytes(ws->gpu[0]);
  }
  if(ws->capacity<ndnbytes(vol))
  { TRY(ndCudaSetCapacity(ws->gpu[0],ndnbytes(vol)));
    TRY(ndCudaSetCapacity(ws->gpu[1],ndnbytes(vol)));
  }
  ndreshape(ws->gpu[0],ndndim(vol),ndshape(vol));
  ndreshape(ws->gpu[1],ndndim(vol),ndshape(vol));
  ndCudaSyncShape(ws->gpu[0]);
  ndCudaSyncShape(ws->gpu[1]);
  return 1;
Error:
  return 0;
}

static unsigned affine_workspace__gpu_resize(affine_workspace *ws, nd_t vol)
{ 
  if(!ws->gpu_xform)
  { TRY(ndreshapev(ndcast(ws->host_xform=ndinit(),nd_f32),2,ndndim(vol)+1,ndndim(vol)+1));
    TRY(ws->gpu_xform=ndcuda(ws->host_xform,0));
  }
  return 1;
Error:
  return 0;
}

/**
 * Anti-aliasing filter. Uses seperable convolutions.
 * Uses double-buffering.  The two buffers are tracked by the filter_workspace.
 * The final buffer is returned as the result.  It is managed by the workspace.
 * The caller should not free it.
 * \todo FIXME: assumes working with at-least-3d data.
 */
nd_t aafilt(nd_t vol, float *transform, filter_workspace *ws)
{ unsigned i,ndim=ndndim(vol);
  for(i=0;i<3;++i) 
    TRY(ws->filters[i]=make_aa_filter(transform[i*(ndim+2)],ws->filters[i])); // ndim+1 for width of transform matrix, ndim+2 to address diagonals
  TRY(filter_workspace__gpu_resize(ws,vol));  
  TRY(ndcopy(ws->gpu[0],vol,0,0));
  for(i=0;i<3;++i)
    TRY(ndconv1(ws->gpu[~i&1],ws->gpu[i&1],ws->filters[i],i,&ws->params));
  ws->i=i&1; // this will be the index of the last destination buffer
  return ws->gpu[ws->i];
Error:
  for(i=0;i<countof(ws->gpu);++i) if(nderror(ws->gpu[i]))
    LOG("\t[nd Error]:"ENDL "\t%s"ENDL,nderror(ws->gpu[i]));
  return 0;
}

nd_t xform(nd_t dst, nd_t src, float *transform, affine_workspace *ws)
{ TRY(affine_workspace__gpu_resize(ws,dst));
  TRY(ndref(ws->host_xform,transform,nd_heap));
  TRY(ndcopy(ws->gpu_xform,ws->host_xform,0,0));
  TRY(ndaffine(dst,src,nddata(ws->gpu_xform),&ws->params));
  return dst;
Error:
  return 0;
}

/**
 * Assume all tiles have the same size.
 */
static nd_t render_leaf(desc_t *desc, aabb_t bbox)
{ nd_t out=0,in=0,t=0;
  size_t i;
  tile_t *tiles;
  TRY(tiles=TileBaseArray(desc->tiles));
  for(i=0;i<TileBaseCount(desc->tiles);++i)
  { // Maybe initialize
    if(!AABBHit(bbox,TileAABB(tiles[i]))) continue;                             // Select hit tiles    
    // Wait to init until a hit is confirmed.
    if(!in)                                                                     // Alloc on first iteration: in, transform
    { unsigned n;
      in=ndioShape(TileFile(tiles[i]));
      TRY(ndref(in,malloc(ndnbytes(in)),nd_heap));
      n=ndndim(in);
      NEW(float,desc->transform,(n+1)*(n+1));
      TRY(set_ref_shape(desc,in));
    }
    if(!out) TRY(out=alloc_vol(desc,bbox,desc->x_nm,desc->y_nm,desc->z_nm));    // Alloc on first iteration: out, must come after set_ref_shape
    // The main idea
    TRY(ndioRead(TileFile(tiles[i]),in));
    compose(desc->transform,bbox,desc->x_nm,desc->y_nm,desc->z_nm,TileTransform(tiles[i]),ndndim(in));
    TRY(t=aafilt(in,desc->transform,&desc->fws));                               // t is on the gpu
    TRY(xform(out,t,desc->transform,&desc->aws));
  }
Finalize:
  ndfree(in);
  return out;
Error:
  release_vol(desc,out);
  out=0;
  goto Finalize;
}

static nd_t make(desc_t *desc, aabb_t bbox, address_t path);
/**
 * \returns 0 on failure, otherwise an nd_t array with the rendered subvolume.
 *          The caller is responsible for releaseing the result with
 *          release_vol().
 */
static nd_t render_node(desc_t *desc, aabb_t bbox, address_t path)
{ 
  nd_t cs[4];
  unsigned i,any=0;
  nd_t out,t;
  aabb_t cboxes[4]={0};
  size_t ndim;
  TRY(AABBBinarySubdivision(cboxes,4,bbox));
  for(i=0;i<4;++i)
  { TRY(address_push(path,i));
    cs[i]=make(desc,cboxes[i],path);
    TRY(address_pop(path));
    any|=(cs[i]!=0);
  }
  if(!any) return 0;

  { int64_t *cshape_nm;
    AABBGet(cboxes[0],0,0,&cshape_nm);
    TRY(out=alloc_vol(desc,bbox,
      2.0f*cshape_nm[0]/ndshape(cs[0])[0],
      2.0f*cshape_nm[1]/ndshape(cs[0])[1],
      cshape_nm[2]/ndshape(cs[0])[2]));
  }
  ndim=ndndim(out);
  for(i=0;i<4;++i)
  { 
    box2box(desc->transform,out,bbox,cs[i],cboxes[i]);
    AABBFree(cboxes[i]);
    TRY(t=aafilt(cs[i],desc->transform,&desc->fws));
    release_vol(desc,cs[i]);
    TRY(xform(out,t,desc->transform,&desc->aws));
  }
  return out;
Error:
  if(out)
    release_vol(desc,out);
  return 0;
}

/// Renders a volume that fills \a bbox fro \a tiles
static nd_t make(desc_t *desc, aabb_t bbox, address_t path)
{ nd_t out=0;
  if(isleaf(desc,bbox))
    out=render_leaf(desc,bbox);
  else
    out=render_node(desc,bbox,path);
  if(out)
    TRY(desc->yield(out,path));
  return out;
Error:
  return 0;
}

// === INTERFACE ===

/**
 * \param[in]   tiles        Source tile database.
 * \param[in]   x_um         Desired pixel size of leaf nodes.
 * \param[in]   y_um         Desired pixel size of leaf nodes.
 * \param[in]   z_um         Desired pixel size of leaf nodes.
 * \param[in]   countof_leaf Maximum size of a leaf node array (in elements).
 * \param[in]   onyield      Callback that handles nodes in the tree when they
 *                           are done being rendered.
 */
unsigned render(tiles_t tiles, float x_um, float y_um, float z_um, size_t countof_leaf, handler_t yield)
{ unsigned ok=1;
  desc_t desc=make_desc(tiles,x_um,y_um,z_um,countof_leaf,yield);
  aabb_t bbox=0;
  address_t path=0;
  TRY(bbox=TileBaseAABB(tiles));
  TRY(path=make_address());
  make(&desc,bbox,path);
Finalize:
  cleanup_desc(&desc);
  AABBFree(bbox);
  free_address(path);
  return ok;
Error:
  ok=0;
  goto Finalize;
}