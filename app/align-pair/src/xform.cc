//#include <Eigen/Core>
#include <Eigen/LU>
#include <stdint.h>
#include "tilebase.h"
#include "nd.h"
#include "xform.h"

#define LOG(...)          fprintf(stderr,__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)

using namespace Eigen;

typedef  Map<Matrix<float,Dynamic,Dynamic,RowMajor> > xform_t;
typedef  Matrix<int64_t,Dynamic,1>     vi64_t;
typedef  vi64_t                        vori_t;
typedef  vi64_t                        vshape_t;

/** returned value is reference to static variable.
    Past references may change value when this function is called again!
 */
static const vori_t vori(aabb_t box_nm)
{ int64_t *v=0;
  size_t ndim=0;
  AABBGet(box_nm,&ndim,&v,0);
  vi64_t r(ndim+2,1); // bbox omits color and affine-lifting dimensions
  r.tail<2>()<<0,1;
  r.segment(0,ndim)=Map<vi64_t>(v,ndim);
  return r;
}

/** returned value is reference to static variable.
    Past references may change value when this function is called again!
 */
static const vshape_t vshape(aabb_t box_nm)
{ int64_t *v=0;
  size_t ndim=0;
  AABBGet(box_nm,&ndim,0,&v);
  vi64_t r(ndim+2,1); // bbox omits color and affine-lifting dimensions
  r.tail<2>()<<0,1;
  r.segment(0,ndim)=Map<vi64_t>(v,ndim);
  return r;
}

static const xform_t xform(tile_t self)
{ xform_t xform(TileTransform(self),5,5);
  return xform;
}

/** makes a bounding box from two opposed corners */
static aabb_t fromCorners(const MatrixXf& r1, const MatrixXf& r2)
{ vi64_t tl,br,s;
  tl=r1.cwiseMin(r2).cast<int64_t>();
  br=r1.cwiseMax(r2).cast<int64_t>();
  s=br-tl;
  aabb_t out=AABBMake(tl.rows()-2); // remove affine and color dims
  AABBSet(out,tl.rows()-2,tl.data(),s.data());
  return out;
}

/** makes a bounding box from an array's shape. */
static aabb_t fromShape(nd_t shape)
{ aabb_t out=AABBMake(3); // hardcoded number of dimensions
  size_t *s=ndshape(shape);
  int64_t o[3]={0,0,0},
         is[3]={s[0],s[1],s[2]};
  return AABBSet(out,3,o,is);
}

/** caller must free returned result with AABBFree */
aabb_t AABBToPx(tile_t self, aabb_t box_nm)
{ MatrixXf T,r1,r2;
  r1=vori(box_nm).cast<float>();
  T=xform(self).inverse();
  r2=(vori(box_nm)+vshape(box_nm)).cast<float>();
  r2.bottomRightCorner<1,1>()<<1;
  aabb_t pbox=fromShape(TileShape(self)),
          out=AABBIntersectIP(fromCorners(T*r1,T*r2),pbox);
  AABBFree(pbox);
  return out;
Error:
  return 0;
}

static size_t* int64_to_static_sizet(int64_t* v,size_t n)
{ size_t i;
  static size_t *out=0,sz=0;
  if(n>sz)
  { sz=1.2*n+50;
    TRY(out=(size_t*)malloc(sizeof(size_t)*sz));
  }
  for(i=0;i<n;++i) out[i]=(size_t)v[i];
Error:
  return out;
}

static size_t* int64_to_new_sizet(int64_t* v,size_t n)
{ size_t i,*out=0;
  TRY(out=(size_t*)malloc(sizeof(size_t)*n));
  for(i=0;i<n;++i) out[i]=(size_t)v[i];
Error:
  return out;
}

//caller must use ndfree to release shape field in returned struct.
struct nd_subarray_t AABBToSubarray(tile_t self, aabb_t box_nm)
{ aabb_t box_px=0;
  struct nd_subarray_t out={0};
  TRY(box_px=AABBToPx(self,box_nm)); // should already be cropped to tile
  { size_t ndim,i;
    int64_t *ori,*shape;
    nd_t s=TileShape(self);
    AABBGet(box_px,&ndim,&ori,&shape);
    ndPushShape(s);
    for(i=0;i<ndim;++i)
      ndShapeSet(s,(unsigned)i,(size_t)shape[i]); // leave the non spatial dimensions the same.  This will get all color channels, for example.
    out.shape=ndheap(s);
    ndPopShape(s);

    TRY(out.ori=(size_t*)malloc(sizeof(size_t)*ndndim(s)));
    for(i=0;i<ndim;++i)
      out.ori[i]=(size_t)ori[i];
    for(;i<ndndim(s);++i)
      out.ori[i]=0;
  }  
Finalize:
  AABBFree(box_px);
  return out;
Error:
  out.ori=0;
  ndfree(out.shape);
  out.shape=0;
  goto Finalize;
}