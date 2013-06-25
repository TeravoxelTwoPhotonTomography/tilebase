/**
 * query-name for tilebase
 * Usage:
 *   query-name <path> <query>
 */

#include "tilebase.h"
#include "src/cache.h"
#include "src/opts.h"
#include "src/search.h"
#include "src/xform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define restrict __restrict

#define ECHO(e)           LOG("--\t%s\n",#e)
#define LOG(...)          fprintf(stderr,__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ ECHO(e); if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)
#define NEW(T,e,N)        TRY((e)=malloc(sizeof(T)*(N)))
#define ZERO(T,e,N)       memset((e),0,sizeof(T)*(N))

static unsigned hit(tile_t *a, void *ctx)
{ aabb_t box=(aabb_t)ctx;
  return AABBHit(TileAABB(*a),box);
}

static unsigned hit_nocorners(tile_t *a, void *ctx)
{ if(hit(a,ctx))
  { aabb_t ref=(aabb_t)ctx,
         other=TileAABB(*a);
    size_t i,n,c=0;
    int64_t *a,*b;
    AABBGet(ref  ,&n,&a,0);
    AABBGet(other,0 ,&b,0);
    for(i=0;i<n;++i)
      c+=(a[i]!=b[i]);
    return (c<=1);
  }
  return 0;
}

static void notunique(const char* query,tile_t **ts, int n)
{ int i;
  LOG("Query did not yield a unique tile: %s\nHits:\n",query);
  for(i=0;i<n;++i)
    LOG("\t%s\n",TilePath(*ts[i]));
}

static aabb_t prnAABB(aabb_t aabb)
{ int64_t *ori,*shape;
  size_t i,ndim;
  if(!AABBGet(aabb,&ndim,&ori,&shape)) return 0;
  if(ndim==0)
  { LOG("\tAABB Empty.\n");
    return aabb;
  }
  LOG("\tAABB [%lld",ori[0]);
  for(i=1;i<ndim;++i)
    LOG(",%lld",ori[i]);
  LOG("]");
  LOG(" to [%lld",ori[0]+shape[0]);
  for(i=1;i<ndim;++i)
    LOG(",%lld",ori[i]+shape[i]);
  LOG("]\n");
  return aabb;
}

/** Makes a new array that is a decimated copy of v.
    Free's v.
  */
nd_t decimate(nd_t v, int factor)
{ nd_t out=ndunknown(v);
  unsigned i,n=ndndim(v);
  if(factor==1) return v;
  ndPushShape(v);
  for(i=0;i<n;++i)
  { out=ndShapeSet(out,i,ndshape(v)[i]/factor);
    ndstrides(v)[i]*=factor;
  }  
  out=ndcopy(ndheap_ip(out),v,0,0);
  ndPopShape(v);
  ndfree(v);
  return out;
}

/* loads and crops. caller should free returned value. 
1. transform bbox back to pixel space using the transform specified by tile
2. use this to determine the shape of the region to read
   keep it power of 2?
3. ndioReadSubarray
*/
nd_t load(tile_t a,aabb_t bbox)
{ struct nd_subarray_t sub=AABBToSubarray(a,bbox);
  AABBFree(prnAABB(AABBToPx(a,bbox)));  
  ndioReadSubarray(TileFile(a),sub.shape,sub.ori,0);
  if(sub.ori) free(sub.ori);
  while(ndndim(sub.shape)>3)
    sub.shape=ndRemoveDim(sub.shape,ndndim(sub.shape)-1);
Error:
  return decimate(ndconvert_ip(sub.shape,nd_f32),1);
}

nd_t thresh(nd_t m,nd_t a,float v)
{ uint8_t* restrict mm=(uint8_t*)nddata(m);
  const float* restrict aa=(float*)nddata(a);
  size_t i;
  const size_t n=ndnelem(a);
  TRY(a && m);
  TRY(ndtype(a)==nd_f32);
  TRY(ndtype(m)==nd_u8);
  TRY(ndkind(a)==nd_heap);
  TRY(ndkind(m)==nd_heap);
  for(i=0;i<n;++i) mm[i]=(uint8_t)(aa[i]>v);
  return m;
Error:
  return 0;
}

int main(int argc,char*argv[])
{ opts_t opts;
  int isok,ecode=0;
  tiles_t tb=0;
  tile_t a,b,*neighbors;
  aabb_t qbox=0;
  size_t nneighbors;
  unsigned (*predicate)(tile_t *a,void *ctx)=hit;
  opts=parsargs(&argc,&argv,&isok);
  if(!isok) return 1;

  if(opts.nocorners)
    predicate=hit_nocorners;
  TRY(tb=TileBaseOpen(opts.path,NULL));
  TRY(a=FindByName(TileBaseArray(tb),TileBaseCount(tb),opts.query[0],notunique));
  TRY(neighbors=TilesFilter(
      TileBaseArray(tb),
      TileBaseCount(tb),
      &nneighbors,
      predicate,
      TileAABB(a)));
  TRY(b=FindByName(neighbors,nneighbors,opts.query[1],notunique)); // only do second search among neighbors
  free(neighbors);

/* TODO */
/*
Add options:
  1. overlap threshold
  2. intensity threshold for mask
  3. eps for aabb based cropping
 */
#if 1
  { nd_t va=0,vb=0,ma=0,mb=0,out=0;
    ndxcorr_plan_t plan=0;
    printf("--- A[B ---\n");
    TRY(va=load(a,TileAABB(b)));
    ndioClose(ndioWrite(ndioOpen("AtoB.h5",NULL,"w"),va));
    ndioClose(ndioWrite(ndioOpen("AtoB.tif",NULL,"w"),va));
    printf("--- B[A ---\n");
    TRY(vb=load(b,TileAABB(a)));
    ndioClose(ndioWrite(ndioOpen("BtoA.h5",NULL,"w"),vb));
    ndioClose(ndioWrite(ndioOpen("BtoA.tif",NULL,"w"),vb));

    TRY(plan=ndxcorr_make_plan(nd_heap,va,vb,10));
    TRY(out=ndheap(ndnormxcorr_output_shape(plan)));

    TRY(thresh(ma=ndmake_type(va,nd_u8),va,0));
    TRY(thresh(mb=ndmake_type(vb,nd_u8),vb,0));
    ndioClose(ndioWrite(ndioOpen("ma.h5",NULL,"w"),ma));
    ndioClose(ndioWrite(ndioOpen("mb.h5",NULL,"w"),mb));

    TRY(ndnormxcorr_masked(out, va, ma, vb, mb, plan));
/*TODO*/ //FindLocalMaxima(out); // build a list of maxima, sortable based on score
    ndioClose(ndioWrite(ndioOpen("out.h5",NULL,"w"),out));

    ndxcorr_free_plan(plan);
    ndfree(out);
    ndfree(ma);
    ndfree(mb);
    ndfree(va);
    ndfree(vb);
  } 
/*TODO*/ //UpdateTransforms();
/*TODO*/// Output();
#endif

  LOG("OK\n");
Finalize:
  TileBaseClose(tb);
  return ecode;
Error:
  LOG("ERROR\n");
  ecode=1;
  goto Finalize;
}