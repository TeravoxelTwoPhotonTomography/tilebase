/**
 * \file
 * Recursively downsample a volume represented by a tile database.
 */
#include <stdio.h>
#include <string.h>
#include "render.h"
#include "filter.h"
#include "xform.h"

#define countof(e) (sizeof(e)/sizeof(*(e)))

#define ENDL        "\n"
#define LOG(...)    fprintf(stderr,__VA_ARGS__) 
#define TRY(e)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto Error;}} while(0)
#define NEW(T,e,N)  TRY((e)=malloc(sizeof(T)*(N)))
#define ZERO(T,e,N) memset((e),0,(N)*sizeof(T))

#define REALLOC(T,e,N)  TRY((e)=realloc((e),sizeof(T)*(N)))

// === ADDRESSING A PATH IN THE TREE ===

void breakme() {LOG(ENDL);}

/**
 * Describes a path through a tree.
 */
struct _address_t
{ int *ids;
  unsigned sz,cap,i;
};

unsigned address_id(address_t self)
{return self->ids[self->i];}

address_t address_next(address_t self)
{ if(!self) return 0;
  ++self->i;
  return (self->i<self->sz)?self:0;
}

static
address_t make_address()
{ address_t out=0;
  NEW(struct _address_t,out,1);
  ZERO(struct _address_t,out,1);
  return out;
Error:
  return 0;
}

static
void free_address(address_t self)
{ if(!self) return;
  if(self->ids) free(self->ids);
  free(self);
}

#if 0
static
address_t address_copy(address_t src)
{ address_t out;
  TRY(out=make_address());
  memcpy(out,src,sizeof(struct _address_t));
  NEW(int,out->ids,out->cap);
  memcpy(out->ids,src->ids,sizeof(int)*src->sz);
  return out;
Error:
  return 0;
}
#endif

static
address_t address_push(address_t self, int i)
{ if(self->sz+1>=self->cap)
    REALLOC(int,self->ids,self->cap=1.2*self->cap+10);
  self->ids[self->sz++]=i;
  return self;
Error:
  return 0;
}

static
address_t address_pop(address_t self)
{ if(self->sz==0) return 0;
  self->sz--;
  return self;
}

// === RENDERING ===

typedef struct _filter_workspace
{ nd_t filters[3];  ///< filter for each axis
  nd_t gpu[2];      ///< double buffer on gpu for serial seperable convolutions
  unsigned capacity;///< capacity of alloc'd gpu buffers
  unsigned i;       ///< current gpu buffer
  nd_conv_params_t params;
} filter_workspace;

typedef struct _affine_workspace
{ nd_t gpu;
  unsigned capacity;
  nd_affine_params_t params;
} affine_workspace;

/// Common arguments and memory context used for building the tree
typedef struct _desc_t
{ tiles_t tiles;
  float x_nm,y_nm,z_nm;
  size_t countof_leaf;
  handler_t yield;

  aabb_t cboxes[4];
  nd_t ref;
  nd_t bufs[5]; //1+4
  int nbufs;
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
  ws->params.boundary_value=0;
};

static desc_t make_desc(tiles_t tiles, float x_um, float y_um, float z_um, size_t countof_leaf, handler_t yield)
{ const float um2nm=1e3;
  unsigned i;

  desc_t out;
  memset(&out,0,sizeof(out));
  out.tiles=tiles;
  out.x_nm=x_um*um2nm;
  out.y_nm=y_um*um2nm;
  out.z_nm=z_um*um2nm;
  out.countof_leaf=countof_leaf;
  out.yield=yield;
  filter_workspace__init(&out.fws);
  affine_workspace__init(&out.aws);

  { int i;
    for(i=0;i<countof(out.cboxes);++i)
      out.cboxes[i]=AABBMake(3);
  }
  return out;
}

static void cleanup_desc(desc_t *desc)
{ size_t i;
  ndfree(desc->ref);
  for(i=0;i<countof(desc->bufs);++i)
    ndfree(desc->bufs[i]);
  if(desc->transform) free(desc->transform);
  for(i=0;i<countof(desc->cboxes);++i)
    AABBFree(desc->cboxes[i]);
}

static desc_t* set_ref_shape(desc_t *desc, nd_t v)
{ nd_t t;
  if(desc->ref) // init first time only
    return desc;
  TRY(ndreshape(ndcast(desc->ref=ndinit(),ndtype(v)),ndndim(v),ndshape(v)));
  { int i;
    ndShapeSet(ndcast(t=ndinit(),ndtype(desc->ref)),0,desc->countof_leaf);
    for(i=0;i<countof(desc->bufs);++i)
      TRY(desc->bufs[i]=ndcuda(t,0));
    desc->nbufs=countof(desc->bufs);
  }
  return desc;
Error:
  LOG("\t[nd Error]:"ENDL "\t%s"ENDL,nderror(t));
  return 0;
}

static unsigned isleaf(const desc_t*const desc, aabb_t bbox)
{ int64_t *shape_nm=0;
  float nm2um=1e-3;
  size_t ndim,c=1;
  AABBGet(bbox,&ndim,0,&shape_nm);
  c*=shape_nm[0]/desc->x_nm;
  c*=shape_nm[1]/desc->y_nm;
  c*=shape_nm[2]/desc->z_nm;
  return c<desc->countof_leaf;
}

static nd_t alloc_vol(desc_t *desc, aabb_t bbox)
{ nd_t v;
  int64_t *shape_nm;
  int64_t  res[]={desc->x_nm,desc->y_nm,desc->z_nm};
  size_t ndim,i;
  TRY(desc->nbufs>0); // must call set_ref_shape first
  TRY(v=desc->bufs[--desc->nbufs]);

  AABBGet(bbox,&ndim,0,&shape_nm);
  for(i=0;i<ndim;++i) // set spatial dimensions
    TRY(ndShapeSet(v,i,shape_nm[i]/res[i]));
  for(;i<ndndim(desc->ref);++i) // other dimensions are same as input
    TRY(ndShapeSet(v,i,ndshape(desc->ref)[i]));
  ndCudaSyncShape(v);

  return v;
Error:
  return 0;
}

static unsigned release_vol(desc_t *desc,nd_t a)
{ TRY(desc->nbufs<countof(desc->bufs));
  desc->nbufs++;
  return 1;
Error:
  return 0;
}

static unsigned filter_workspace__gpu_resize(filter_workspace *ws, nd_t vol)
{ if(!ws->gpu[0])
  { TRY(ws->gpu[0]=ndcuda(vol,0));
    TRY(ws->gpu[1]=ndcuda(vol,0));
    ws->capacity=ndnbytes(ws->gpu[0]);
  }
  if(ws->capacity<ndnbytes(vol))
  { TRY(ndCudaSetCapacity(ws->gpu[0],ndnbytes(vol)));
    TRY(ndCudaSetCapacity(ws->gpu[1],ndnbytes(vol)));
  }
  return 1;
Error:
  return 0;
}

static unsigned affine_workspace__gpu_resize(affine_workspace *ws, nd_t vol)
{ if(!ws->gpu)
  { TRY(ws->gpu=ndcuda(vol,0));
    ws->capacity=ndnbytes(ws->gpu);
  }
  if(ws->capacity<ndnbytes(vol))
    TRY(ndCudaSetCapacity(ws->gpu,ndnbytes(vol)));
  return 1;
Error:
  return 0;
}

/**
 * Anti-aliasing filter. 
 * \todo FIXME: assumes working with at-least-3d data.
 */
nd_t aafilt(nd_t vol, float *transform, filter_workspace *ws)
{ unsigned i,ndim=ndndim(vol);
  for(i=0;i<3;++i)
    TRY(ws->filters[i]=make_aa_filter(transform[i*(ndim+2)],ws->filters[i])); // ndim+1 for width of transform matrix, ndim+2 to address diagonals
  TRY(filter_workspace__gpu_resize(ws,vol));
  TRY(ndCudaCopy(ws->gpu[0],vol));
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
  TRY(ndaffine(ws->gpu,src,transform,&ws->params));
  ndCudaCopy(dst,ws->gpu);
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
      TRY(ndref(in,malloc(ndnbytes(in)),ndnelem(in)));
      n=ndndim(in);
      NEW(float,desc->transform,(n+1)*(n+1));
      TRY(set_ref_shape(desc,in));
    }
    if(!out) TRY(out=alloc_vol(desc,bbox));                                     // Alloc on first iteration: out, must come after set_ref_shape
    // The main idea
    TRY(ndioRead(TileFile(tiles[i]),in));
    compose(desc->transform,bbox,desc->x_nm,desc->y_nm,desc->z_nm,TileTransform(tiles[i]),ndndim(in));
    TRY(t=aafilt(in,desc->transform,&desc->fws));                               // t is on the gpu
    TRY(xform(out,t,desc->transform,&desc->aws));
  }
Finalize:
  if(nddata(in)) free(nddata(in));
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

  TRY(AABBBinarySubdivision(desc->cboxes,4,bbox));
  for(i=0;i<4;++i)
  { TRY(address_push(path,i));
    cs[i]=make(desc,desc->cboxes[i],path);
    TRY(address_pop(path));
    any|=(cs[i]!=0);
  }
  if(!any) return 0;
  TRY(out=alloc_vol(desc,bbox));
  for(i=0;i<4;++i)
  { box2box(desc->transform,out,bbox,cs[i],desc->cboxes[i]);
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
  if(out) desc->yield(out,path);
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
  aabb_t bbox;
  address_t path;
  TRY(bbox=TileBaseAABB(tiles));
  TRY(path=make_address());
  make(&desc,bbox,path);
Finalize:
  cleanup_desc(&desc);
  free(bbox);
  free(path);
  return ok;
Error:
  ok=0;
  goto Finalize;
}