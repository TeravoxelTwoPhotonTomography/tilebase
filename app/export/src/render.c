/**
 * \file
 * Recursively downsample a volume represented by a tile database.
 *
 * \todo refactor to sepererate tree traversal from the rendering bits.
 */
#include <stdio.h>
#include <string.h>
#include "render.h"
#include "filter.h"
#include "xform.h"
#include "address.h"
#include <math.h> //for sqrt
#include "cuda_runtime.h" // for cudaGetMemInfo
#include "tictoc.h" // for profiling

#ifdef _MSC_VER
#define alloca   _alloca
#endif

#define countof(e) (sizeof(e)/sizeof(*(e)))

#define ENDL          "\n"
#define LOG(...)      fprintf(stderr,__VA_ARGS__) 
#define TRY(e)        do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto Error;}} while(0)
#define NEW(T,e,N)    TRY((e)=(T*)malloc(sizeof(T)*(N)))
#define ALLOCA(T,e,N) TRY((e)=(T*)alloca(sizeof(T)*(N)))
#define ZERO(T,e,N)   memset((e),0,(N)*sizeof(T))

#define REALLOC(T,e,N)  TRY((e)=realloc((e),sizeof(T)*(N)))

#define DEBUG
#define PROFILE_MEMORY
#define ENABLE_PROGRESS_OUTPUT
//#define DEBUG_DUMP_IMAGES
#define PROFILE

#ifdef DEBUG
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif

#ifdef ENABLE_PROGRESS_OUTPUT
#define PROGRESS(...) LOG(__VA_ARGS__)
#else
#define PROGRESS(...)
#endif

#ifdef PROFILE_MEMORY
nd_t ndcuda_log(nd_t vol, void *s)
{ unsigned i;
  LOG("NDCUDA ALLOC: %g MB [%llu",ndnbytes(vol)*1e-6,(unsigned long long)ndshape(vol)[0]);
  for(i=1; i<ndndim(vol);++i)
    LOG(",%llu",(unsigned long long)ndshape(vol)[i]);
  LOG("]"ENDL);
  return ndcuda(vol,s);
}
#define NDCUDA(v,s) ndcuda_log(v,s)
#else
#define NDCUDA(v,s) ndcuda(v,s)
#endif // PROFILE_MEMORY


#ifdef DEBUG_DUMP_IMAGES
#define DUMP(...) dump(__VA_ARGS__)
#else
#define DUMP(...)
#endif

#ifdef PROFILE
#define TIME(e) do{ TicTocTimer __t__=tic(); e; LOG("%7.3f s - %s"ENDL,toc(&__t__),#e); }while(0)
#else
#define TIME(e) e
#endif
// === ADDRESSING A PATH IN THE TREE ===

static void breakme() {LOG(ENDL);}

void dump(const char *filename,nd_t a)
{ LOG("Writing %s"ENDL,filename);
  ndioClose(ndioWrite(ndioOpen(filename,NULL,"w"),a));
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
{ nd_t host_xform,gpu_xform;
  nd_affine_params_t params;
} affine_workspace;

/// Common arguments and memory context used for building the tree
typedef struct _desc_t desc_t;
struct _desc_t
{ /* PARAMETERS */
  tiles_t tiles;
  float x_nm,y_nm,z_nm,voxvol_nm3;
  size_t nchildren; // subdivision factor.  must be power of 2. 4 means subdivision will be on x and y, 8 is x,y, and z
  size_t countof_leaf;
  void *args; // extra arguments to pass to yield()
  handler_t yield;

  /* WORKSPACE */
  nd_t ref;
  int  nbufs;
  nd_t *bufs; //Need 1 for each node on path in tree - so pathlength(root,leaf)
  int  *inuse;
  filter_workspace fws;
  affine_workspace aws;
  float *transform;

  /* INTERFACE */
  /* Returns an array that fills the bounding box \a bbox that corresponds to 
     the a node on the subdivision tree specified by \a path.

     This usually involves a recursive descent of the tree, so this function is
     expected to call itself.  Various helper functions below help with the
     traversal and manage the desc_t workspace.

     The implementation can call desc->yield() to pass data out during the 
     tree traversal.  For example, this is useful for saving each hierarchically
     downsampled volume to disk as a root node is rendered.
   */
  nd_t (*make)(desc_t *desc,aabb_t bbox,address_t path);
};

//
// Forward declare interface functions. See desc_t comments for description of the interface.
//

// Hierarchical downsampling in memory.
static nd_t render_child(desc_t *desc, aabb_t bbox, address_t path);
static nd_t render_leaf(desc_t *desc, aabb_t bbox, address_t path);
static nd_t render_child_to_parent(desc_t *desc,aabb_t bbox, address_t path, nd_t child, aabb_t cbox, nd_t workspace);

static void filter_workspace__init(filter_workspace *ws)
{ memset(ws,0,sizeof(*ws));
  ws->params.boundary_condition=nd_boundary_replicate;
}

static void affine_workspace__init(affine_workspace *ws)
{ memset(ws,0,sizeof(*ws));
  ws->params.boundary_value=0x8000; // MIN_I16 - TODO: hardcoded here...should be an option
};

static double boundary_value(nd_t vol)
{
  switch(ndtype(vol))
  { 
    case nd_u8:
    case nd_u16:
    case nd_u32:
    case nd_u64:
    case nd_f32:
    case nd_f64: return 0; break;
    case nd_i8:  return 0x80; break; 
    case nd_i16: return 0x8000; break;
    case nd_i32: return 0x80000000; break; 
    case nd_i64: return 0x8000000000000000LL; break; 
    default: return 0;
  }
}

static void affine_workspace__set_boundary_value(affine_workspace* ws,nd_t vol)
{ ws->params.boundary_value=boundary_value(vol);
}

static desc_t make_desc(tiles_t tiles, double voxel_um[3], size_t nchildren, size_t countof_leaf, handler_t yield, void* args)
{ const float um2nm=1e3;

  desc_t out;
  memset(&out,0,sizeof(out));
  out.tiles=tiles;
  out.x_nm=voxel_um[0]*um2nm;
  out.y_nm=voxel_um[1]*um2nm;
  out.z_nm=voxel_um[2]*um2nm;
  out.voxvol_nm3=out.x_nm*out.y_nm*out.z_nm;
  out.nchildren=nchildren;
  out.countof_leaf=countof_leaf;
  out.args=args;
  out.yield=yield;
  filter_workspace__init(&out.fws);
  affine_workspace__init(&out.aws);

  out.make=render_child;
  return out;
}

static void cleanup_desc(desc_t *desc)
{ size_t i;
  ndfree(desc->ref);
  for(i=0;i<desc->nbufs;++i)
    ndfree(desc->bufs[i]);
  if(desc->transform) free(desc->transform);
}

static unsigned isleaf(const desc_t*const desc, aabb_t bbox)
{ int64_t c=AABBVolume(bbox)/(double)desc->voxvol_nm3;
  return c<(int64_t)desc->countof_leaf;
}

/// Count path length from the current node to a leaf
static int pathlength(desc_t *desc, aabb_t bbox)
{ int i,n=0;
  aabb_t *cboxes=0;
  ALLOCA(aabb_t,cboxes,desc->nchildren);
  ZERO(  aabb_t,cboxes,desc->nchildren);
  if(isleaf(desc,bbox)) return 1;
  AABBBinarySubdivision(cboxes,desc->nchildren,bbox);
  n=pathlength(desc,cboxes[0]);
  for(i=0;i<desc->nchildren;++i) AABBFree(cboxes[i]);
  return n+1;
Error:
  return 0;
}

/** This and set_ref_shape() allocate space required to render the subdivision tree. */
static int preallocate(desc_t *desc, aabb_t bbox)
{ size_t n;
  TRY(n=pathlength(desc,bbox));
  NEW(nd_t,desc->bufs,n);
  ZERO(nd_t,desc->bufs,n);
  NEW(int,desc->inuse,n);
  ZERO(int,desc->inuse,n);
  desc->nbufs=n;
  return 1;
Error:
  return 0;
}

static int preallocate_for_render_one_target(desc_t *desc, aabb_t bbox)
{ size_t n=2;
  //TRY(n=pathlength(desc,bbox));
  NEW(nd_t,desc->bufs,n);
  ZERO(nd_t,desc->bufs,n);
  NEW(int,desc->inuse,n);
  ZERO(int,desc->inuse,n);
  desc->nbufs=n;
  return 1;
Error:
  return 0;
}

static desc_t* set_ref_shape(desc_t *desc, nd_t v)
{ nd_t t;
  if(desc->ref) // init first time only
  { TRY(ndreshape(ndcast(desc->ref,ndtype(v)),ndndim(v),ndshape(v))); // update reference shape
    return desc;
  }
  TRY(ndreshape(ndcast(desc->ref=ndinit(),ndtype(v)),ndndim(v),ndshape(v)));
  { int i;    // preallocate gpu bufs with pixel type corresponding to input
    TRY(ndShapeSet(ndcast(t=ndinit(),ndtype(desc->ref)),0,desc->countof_leaf));
    for(i=0;i<desc->nbufs;++i)
      TRY(desc->bufs[i]=NDCUDA(t,0));
  }
  return desc;
Error:
  LOG("\t[nd Error]:"ENDL "\t%s"ENDL,nderror(t));
  return 0;
}

static int sum(int n, const int*const v)
{ int i,o=0;
  for(i=0;i<n;++i) o+=v[i];
  return o;
}

static unsigned same_shape_by_res(nd_t v, int64_t*restrict shape_nm, int64_t*restrict res, size_t ndim)
{ size_t i,*vs;
  vs=ndshape(v);
  for(i=0;i<ndim;++i)
    if(vs[i]!=(shape_nm[i]/res[i]))
      return 0;
  return 1;
}

static nd_t alloc_vol(desc_t *desc, aabb_t bbox, int64_t x_nm, int64_t y_nm, int64_t z_nm)
{ nd_t v;
  int64_t *shape_nm;
  int64_t  res[]={x_nm,y_nm,z_nm};
  size_t ndim,i,j;
  int resize=1;
  AABBGet(bbox,&ndim,0,&shape_nm);
  for(i=0;i<desc->nbufs && desc->inuse[i];++i); // search for first unused - preferably with correct shape
  for(j=i;j<desc->nbufs;++j)
  { if(!desc->inuse[j] && same_shape_by_res(desc->bufs[j],shape_nm,res,ndim))
    { i=j;
      resize=0;
      break;
    }
  }
  TRY(i<desc->nbufs);                           // check that one was available  
  TRY(v=desc->bufs[i]);                         // ensure bufs was init'd - see set_ref_shape()
  desc->inuse[i]=1;
  DBG("    alloc_vol(): [%3u] buf=%u"ENDL,(unsigned)sum(desc->nbufs,desc->inuse),(unsigned)i);
  if(resize)
  { for(i=0;i<ndim;++i) // set spatial dimensions
      TRY(ndShapeSet(v,i,shape_nm[i]/res[i]));
    for(;i<ndndim(desc->ref);++i) // other dimensions are same as input
      TRY(ndShapeSet(v,(unsigned)i,ndshape(desc->ref)[i]));
    TRY(ndCudaSyncShape(v)); // expensive...worth avoiding
  }
  TRY(ndfill(v,desc->aws.params.boundary_value));  
  return v;
Error:
  return 0;
}

static unsigned release_vol(desc_t *desc,nd_t a)
{ size_t i;
  if(!a) return 0;
  for(i=0;i<desc->nbufs && desc->bufs[i]!=a;++i) {} // search for buffer corresponding to a
  DBG("    release_vol(): buf=%u"ENDL,(unsigned)i);
  if(i>=desc->nbufs)                                // if a is not from the managed pool, just free it
  { ndfree(a);
    return 1;
  }
  TRY(desc->inuse[i]);                              // ensure it was marked inuse. (sanity check)
  desc->inuse[i]=0;
  return 1;
Error:
  return 0;
}

static unsigned same_shape(nd_t a, nd_t b)
{ size_t i;
  size_t *sa,*sb;
  if(ndndim(a)!=ndndim(b)) return 0;
  sa=ndshape(a);
  sb=ndshape(b);
  for(i=0;i<ndndim(a);++i)
    if(sa[i]!=sb[i]) return 0;
  return 1;
}

static unsigned filter_workspace__gpu_resize(filter_workspace *ws, nd_t vol)
{ 
  { size_t free,total;
    cudaMemGetInfo(&free,&total);
    LOG("GPU Mem:\t%6.2f free\t%6.2f total\n",free/1e6,total/1e6);
  }
  
  if(!ws->gpu[0])
  { TRY(ws->gpu[0]=NDCUDA(vol,0));
    TRY(ws->gpu[1]=NDCUDA(vol,0));
    ws->capacity=(unsigned)ndnbytes(ws->gpu[0]);
  }
#if 0 //don't need this because ndCudaSyncShape sets capacity if necessary
  if(ws->capacity<ndnbytes(vol))
  { TRY(ndCudaSetCapacity(ws->gpu[0],ndnbytes(vol)));
    TRY(ndCudaSetCapacity(ws->gpu[1],ndnbytes(vol)));
    ws->capacity=ndnbytes(vol);
  }
#endif
  if(!same_shape(ws->gpu[0],vol))
  { ndreshape(ws->gpu[0],ndndim(vol),ndshape(vol));
    ndreshape(ws->gpu[1],ndndim(vol),ndshape(vol));
    ndCudaSyncShape(ws->gpu[0]); // the cuda transfer can get expensive so it's worth avoiding these calls
    ndCudaSyncShape(ws->gpu[1]);
    ws->capacity=(unsigned)ndnbytes(ws->gpu[0]);
  }
  return 1;
Error:
  return 0;
}

static unsigned affine_workspace__gpu_resize(affine_workspace *ws, nd_t vol)
{ 
  if(!ws->gpu_xform)
  { TRY(ndreshapev(ndcast(ws->host_xform=ndinit(),nd_f32),2,ndndim(vol)+1,ndndim(vol)+1));
    TRY(ws->gpu_xform=NDCUDA(ws->host_xform,0));
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
{ unsigned i=0,ndim=ndndim(vol);
  for(i=0;i<3;++i) 
    TIME(TRY(ws->filters[i]=make_aa_filter(transform[i*(ndim+2)],ws->filters[i]))); // ndim+1 for width of transform matrix, ndim+2 to address diagonals
  TIME(TRY(filter_workspace__gpu_resize(ws,vol)));
  DUMP("aafilt-vol.%.tif",vol);
  TIME(TRY(ndcopy(ws->gpu[0],vol,0,0)));
  DUMP("aafilt-src.%.tif",ws->gpu[0]);
  for(i=0;i<3;++i)
    TIME(TRY(ndconv1(ws->gpu[~i&1],ws->gpu[i&1],ws->filters[i],i,&ws->params)));
  ws->i=i&1; // this will be the index of the last destination buffer
  DUMP("aafilt-dst.%.tif",ws->gpu[ws->i]);
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
  DUMP("xform-src.%.tif",src);
  TRY(ndaffine(dst,src,nddata(ws->gpu_xform),&ws->params));
  DUMP("xform-dst.%.tif",dst);
  return dst;
Error:
  if(dst) LOG("\t[nd Error]:"ENDL "\t%s"ENDL,nderror(dst));
  return 0;
}

static int any_tiles_in_box(tile_t *tiles, size_t ntiles, aabb_t bbox)
{ size_t i;
  for(i=0;i<ntiles;++i)
    if(AABBHit(bbox,TileAABB(tiles[i])))
      return 1;
  return 0;
}

/*
   Tile split for limited memory on the gpu.
   Division is linearly spaced on z.
*/
typedef struct _subdiv_t
{ int i,n,d;
  int64_t dz_nm,dz_px,zmax;
  float *transform;
  nd_t   carray;
} *subdiv_t;
static subdiv_t  make_subdiv(nd_t in, float *transform, int ndim)
{ subdiv_t ctx=0;
  size_t free,total,factor;
  TRY(ndndim(in)>3); // assume there's a z.
  NEW(struct _subdiv_t,ctx,1);
  ZERO(struct _subdiv_t,ctx,1);
  cudaMemGetInfo(&free,&total);
  ctx->n=(ndnbytes(in)*2+free)/free; /* need 2 copies of the subarray. This is a ceil of req.bytes/free.bytes */
  ctx->transform=transform;
  ctx->d=ndim;
  ctx->zmax =ndshape(in)[2];
  if(ctx->n==1) // nothing to do - single iteration
  { ctx->carray=in;
    return ctx;
  }
  TRY(ctx->carray=ndreshape(
                ndcast(
                  ndref(ndinit(),nddata(in),ndkind(in)),
                  ndtype(in)),
                ndndim(in),ndshape(in)));
  ctx->dz_px=1+ndshape(ctx->carray)[2]/ctx->n; // make blocks big enough to account for rounding errors, last block will be small
  ndshape(ctx->carray)[2]=ctx->dz_px; // don't adjust strides, will get copied to a correctly formatted array on the gpu
  ctx->dz_nm=ctx->dz_px*transform[(ndim+2)*2];
  return ctx;
Error:
  return 0;
}
static void free_subdiv(subdiv_t ctx) 
{ if(ctx)
  { if(ctx->n>1)
      ndfree(ndref(ctx->carray,0,nd_unknown_kind));
    free(ctx);
  }
};
static nd_t subdiv_vol(subdiv_t ctx)       {return ctx->carray;}
static float* subdiv_xform(subdiv_t ctx)   {return ctx->transform;}
static int  next_subdivision(subdiv_t ctx)
{ if(++ctx->i<ctx->n)
  { TRY(ctx->n>1); // sanity check..remove once confirmed
    if((ctx->i+1)*ctx->dz_px>ctx->zmax) // adjust last block if necessary
      ndshape(ctx->carray)[2]=ctx->zmax-ctx->i*ctx->dz_px; // don't adjust strides, will get copied to a correctly formatted array on the gpu
    ndoffset(ctx->carray,2,ctx->dz_px);
    ctx->transform[(ctx->d+1)*2+ctx->d]+=ctx->dz_nm;
    return 1;
  }
Error:
  return 0;
}

/**
 * Does not assume all tiles have the same size. (fixed: ngc)
 */
static nd_t render_leaf(desc_t *desc, aabb_t bbox, address_t path)
{ nd_t out=0,in=0,t=0;
  size_t i;
  tile_t *tiles;
  subdiv_t subdiv=0;
  TRY(tiles=TileBaseArray(desc->tiles));
  for(i=0;i<TileBaseCount(desc->tiles);++i)
  { // Maybe initialize
    if(!AABBHit(bbox,TileAABB(tiles[i]))) continue;                             // Select hit tiles    
    PROGRESS(".");
    // Wait to init until a hit is confirmed.
    if(!in)                                                                     // Alloc on first iteration: in, transform
    { unsigned n;
      in=ndioShape(TileFile(tiles[i]));
      TRY(ndref(in,malloc(ndnbytes(in)),nd_heap));
      n=ndndim(in);
      NEW(float,desc->transform,(n+1)*(n+1));   // FIXME: pretty sure this is a memory leak.  transform get's init'd for each leaf without being freed
      TRY(set_ref_shape(desc,in));
       affine_workspace__set_boundary_value(&desc->aws,in);
    } 
    if(!same_shape(in,TileShape(tiles[i]))) // maybe resize "in"
    { nd_t s=TileShape(tiles[i]);
      if(ndnbytes(in)<ndnbytes(s))
        TRY(ndref(in,realloc(nddata(in),ndnbytes(s)),nd_heap));
      TRY(ndcast(ndreshape(in,ndndim(s),ndshape(s)),ndtype(s)));
    }
    if(!out) TRY(out=alloc_vol(desc,bbox,desc->x_nm,desc->y_nm,desc->z_nm));    // Alloc on first iteration: out, must come after set_ref_shape
    // The main idea
    TIME(TRY(ndioRead(TileFile(tiles[i]),in)));
    TRY(subdiv=make_subdiv(in,TileTransform(tiles[i]),ndndim(in)));
    do
    { TIME(compose(desc->transform,bbox,desc->x_nm,desc->y_nm,desc->z_nm,subdiv_xform(subdiv),ndndim(in)));
      TIME(TRY(t=aafilt(subdiv_vol(subdiv),desc->transform,&desc->fws)));                         // t is on the gpu
      TIME(TRY(xform(out,t,desc->transform,&desc->aws)));
    } while(next_subdivision(subdiv));
  }
Finalize:
  PROGRESS(ENDL);
  free_subdiv(subdiv);
  ndfree(in);
  return out;
Error:
  release_vol(desc,out);
  out=0;
  goto Finalize;
}

static nd_t render_child_to_parent(desc_t *desc,aabb_t bbox, address_t path, nd_t child, aabb_t cbox, nd_t workspace)
{ nd_t t,out=workspace;   
  // maybe allocate output
  if(!out)
  { int64_t *cshape_nm;
    unsigned n=desc->nchildren;
    const float s[]={ (n>>=1)?2.0f:1.0f,
                      (n>>=1)?2.0f:1.0f,
                      (n>>=1)?2.0f:1.0f };
    AABBGet(cbox,0,0,&cshape_nm);
    TRY(out=alloc_vol(desc,bbox,
      s[0]*cshape_nm[0]/ndshape(child)[0],
      s[1]*cshape_nm[1]/ndshape(child)[1],
      s[2]*cshape_nm[2]/ndshape(child)[2]));
  }
  // paste child into output
  box2box(desc->transform,out,bbox,child,cbox);
  TRY(t=aafilt(child,desc->transform,&desc->fws));
  TRY(xform(out,t,desc->transform,&desc->aws));
  return out;
Error:
  return 0;
}

/**
 * \returns 0 on failure, otherwise an nd_t array with the rendered subvolume.
 *          The caller is responsible for releaseing the result with
 *          release_vol().
 */
static nd_t render_node(desc_t *desc, aabb_t bbox, address_t path)
{   
  unsigned i;
  nd_t out=0;
  aabb_t *cboxes=0;
  ALLOCA(aabb_t,cboxes,desc->nchildren);
  ZERO(  aabb_t,cboxes,desc->nchildren);
  TRY(AABBBinarySubdivision(cboxes,desc->nchildren,bbox));
  for(i=0;i<desc->nchildren;++i)
  { nd_t t,c=0;
    TRY(address_push(path,i));
    c=desc->make(desc,cboxes[i],path);
    TRY(address_pop(path));
    if(!c) continue;
    out=render_child_to_parent(desc,bbox,path,c,cboxes[i],out);
    release_vol(desc,c);
    AABBFree(cboxes[i]);
  }
  return out;
Error:
  if(out)
    release_vol(desc,out);
  return 0;
}

/// Renders a volume that fills \a bbox fro \a tiles
static nd_t render_child(desc_t *desc, aabb_t bbox, address_t path)
{ nd_t out=0;
  DBG("--- Address: %-20u ---"ENDL, (unsigned)address_to_int(path,10));
  if(isleaf(desc,bbox))
    out=render_leaf(desc,bbox,path);
  else
    out=render_node(desc,bbox,path);
  if(out)
    TRY(desc->yield(out,path,desc->args));
  return out;
Error:
  return 0;
}

//
// JUST THE ADDRESS SEQUENCE 
// yielded in oreder of dependency, leaves (no dependencies) first.
//
static nd_t addr_seq__child(desc_t *desc, aabb_t bbox, address_t path)
{
  if(!isleaf(desc,bbox))
    render_node(desc,bbox,path);
  if(any_tiles_in_box(TileBaseArray(desc->tiles),TileBaseCount(desc->tiles),bbox)) // cull empty nodes
    desc->yield(0,path,desc->args);
  return 0;
}
static nd_t addr_seq__compose_child(desc_t *desc,aabb_t bbox, address_t path, nd_t child, aabb_t cbox, nd_t workspace)
{ return 0;}
static void setup_print_addresses(desc_t *desc)
{ desc->make=addr_seq__child;
}

//
// RENDER JUST THE TARGET ADDRESS
// target address
static address_t target__addr=0;
static loader_t target__load_func=0;
static nd_t target__load(desc_t *desc, aabb_t bbox, address_t path)
{ unsigned n;
  nd_t out=target__load_func?target__load_func(path):0;
  if(out)
  { n=ndndim(out);
    NEW(float,desc->transform,(n+1)*(n+1));   // FIXME: pretty sure this is a memory leak.  transform get's init'd for each leaf without being freed
    TRY(set_ref_shape(desc,out));
    affine_workspace__set_boundary_value(&desc->aws,out);
  }
  return out;
Error:
  return 0;
}
static nd_t target__get_child(desc_t *desc, aabb_t bbox, address_t path)
{ nd_t out=0;
  if(address_eq(path,target__addr))
  { if(isleaf(desc,bbox))
      out=render_leaf(desc,bbox,path);
    else
    { desc->make=target__load;
      out=render_node(desc,bbox,path);
      desc->make=target__get_child;
    }

    if(out)
      TRY(desc->yield(out,path,desc->args));
  } else
  { if(!isleaf(desc,bbox))
      render_node(desc,bbox,path);
  }
  
  return out;
Error:
  return 0;
}
static void target__setup(desc_t *desc, address_t target, loader_t loader)
{ target__addr=target;
  target__load_func=loader;
  desc->make=target__get_child;
}

// === INTERFACE ===

/** Select a subvolume from the total data set using fractional coordinates.  
  */
aabb_t AdjustTilesBoundingBox(tiles_t tiles, double o[3], double s[3])
{ aabb_t bbox;
  int64_t *ori,*shape;
  int i;
  TRY(bbox=TileBaseAABB(tiles));
  AABBGet(bbox,0,&ori,&shape);
  for(i=0;i<3;++i) ori[i]  =ori[i]+o[i]*shape[i];
  for(i=0;i<3;++i) shape[i]=       s[i]*shape[i];
  return bbox;
Error:
  return 0;
}

/**
 * \param[in]   tiles        Source tile database.
 * \param[in]   voxel_um     Desired voxel size of leaf nodes (x,y, and z).
 * \param[in]   ori          Output box origin as a fraction of the total bounding box (0 to 1; x,y and z).
 * \param[in]   size         Output box width as a fraction of the total bounding box (0 to 1; x, y and z).
 * \param[in]   countof_leaf Maximum size of a leaf node array (in elements).
 * \param[in]   yield        Callback that handles nodes in the tree when they
 *                           are done being rendered.
 * \param[in]   args         Additional arguments to be passed to yeild.
 */
unsigned render(tiles_t tiles, double voxel_um[3], double ori[3], double size[3], size_t nchildren, size_t countof_leaf, handler_t yield, void* args)
{ unsigned ok=1;
  desc_t desc=make_desc(tiles,voxel_um,nchildren,countof_leaf,yield,args);
  aabb_t bbox=0;
  address_t path=0;
  TRY(bbox=AdjustTilesBoundingBox(tiles,ori,size));
  TRY(preallocate(&desc,bbox));
  TRY(path=make_address());
  desc.make(&desc,bbox,path);
Finalize:
  cleanup_desc(&desc);
  AABBFree(bbox);
  free_address(path);
  return ok;
Error:
  ok=0;
  goto Finalize;
}

/**
 * \param[in]   tiles        Source tile database.
 * \param[in]   voxel_um     Desired voxel size of leaf nodes (x,y, and z).
 * \param[in]   ori          Output box origin as a fraction of the total bounding box (0 to 1; x,y and z).
 * \param[in]   size         Output box width as a fraction of the total bounding box (0 to 1; x, y and z).
 * \param[in]   countof_leaf Maximum size of a leaf node array (in elements).
 * \param[in]   yield        Callback that handles nodes in the tree when they
 *                           are ready.
 * \param[in]   args         Additional arguments to be passed to yeild.
 */
unsigned addresses(tiles_t tiles, double voxel_um[3], double ori[3], double size[3], size_t nchildren, size_t countof_leaf, handler_t yield, void* args)
{ unsigned ok=1;
  desc_t desc=make_desc(tiles,voxel_um,nchildren,countof_leaf,yield,args);
  aabb_t bbox=0;
  address_t path=0;
  TRY(bbox=AdjustTilesBoundingBox(tiles,ori,size));
//TRY(preallocate(&desc,bbox));
  TRY(path=make_address());
  setup_print_addresses(&desc);
  desc.make(&desc,bbox,path);
Finalize:
  cleanup_desc(&desc);
  AABBFree(bbox);
  free_address(path);
  return ok;
Error:
  ok=0;
  goto Finalize;
}

/**
 * \param[in]   tiles        Source tile database.
 * \param[in]   voxel_um     Desired voxel size of leaf nodes (x,y, and z).
 * \param[in]   ori          Output box origin as a fraction of the total bounding box (0 to 1; x,y and z).
 * \param[in]   size         Output box width as a fraction of the total bounding box (0 to 1; x, y and z).
 * \param[in]   countof_leaf Maximum size of a leaf node array (in elements).
 * \param[in]   yield        Callback that handles nodes in the tree when they
 *                           are ready.
 * \param[in]   yield_args   Additional arguments to be passed to yeild.
 * \param[in]   loader       Function that loads the data at the node specified by an address.
 * \param[in]   target       The address of the tree to target.
 */
unsigned render_target(tiles_t tiles, double voxel_um[3], double ori[3], double size[3], size_t nchildren, size_t countof_leaf, handler_t yield, void *yield_args, loader_t loader, address_t target)
{ unsigned ok=1;
  desc_t desc=make_desc(tiles,voxel_um,nchildren,countof_leaf,yield,yield_args);
  aabb_t bbox=0;
  address_t path=0;
  TRY(bbox=AdjustTilesBoundingBox(tiles,ori,size));
  TRY(preallocate_for_render_one_target(&desc,bbox));
  TRY(path=make_address());
  target__setup(&desc,target,loader);
  desc.make(&desc,bbox,path);
Finalize:
  cleanup_desc(&desc);
  AABBFree(bbox);
  free_address(path);
  return ok;
Error:
  ok=0;
  goto Finalize;
}
