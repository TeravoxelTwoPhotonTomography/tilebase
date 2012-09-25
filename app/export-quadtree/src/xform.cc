/**
 * Computing transforms.
 */
#include <stdlib.h>
#include <stdint.h>
#include "src/aabb.h"
#include "nd.h"
#include <Eigen/Core>
#include <Eigen/LU>
using namespace Eigen;
#include <iostream>
using namespace std;

#ifndef restrict
  #define restrict __restrict
#endif

// output transform should be T s.t. rs=T*rd
// so, if r=Ts*rs=Td*rd
//   T = inv(Ts)*Td

/**
 * \param[in,out] out   Must be preallocated with at least (ndim+1)*(ndim+1) 
 *                      elements
 */
extern "C"
void compose(float *restrict out,
             aabb_t bbox,
             float sx, float sy, float sz, // scale in aabb units
             float *restrict transform, unsigned ndim)
{ 
  MatrixXf                         dst2world(ndim+1,ndim+1);
//  Map<MatrixXf,Unaligned,Stride<1,Dynamic> > src2world(transform,ndim+1,ndim+1);
  Map<Matrix<float,Dynamic,Dynamic,RowMajor> > src2world(transform,ndim+1,ndim+1);
  Map<Matrix<float,Dynamic,Dynamic,RowMajor> > T(out,ndim+1,ndim+1);

  int64_t      *s;
  AABBGet(bbox,0,0,&s);
  dst2world.setIdentity();
  dst2world.block<3,1>(0,ndim)<<(float)s[0],(float)s[1],(float)s[2];
  dst2world.block<3,3>(0,0).diagonal()<<sx,sy,sz;  

  T=(src2world.inverse()*dst2world).eval();
  #define show(e) cout<<#e" is "<<endl<<e<<endl<<endl
  show(dst2world);
  show(src2world);
  show(src2world.inverse());
  show(T);
  #undef show
}

extern "C"
void box2box(float *restrict out,
             nd_t dst, aabb_t dstbox,
             nd_t src, aabb_t srcbox)
{ size_t d;
  int64_t *shape;
  
  AABBGet(srcbox,&d,0,&shape);
  MatrixXf src2world(d+1,ndndim(src)+1);
  src2world.setIdentity().block<3,3>(0,0).diagonal()
    << (float)shape[0]/(float)ndshape(src)[0],
       (float)shape[1]/(float)ndshape(src)[1],
       (float)shape[2]/(float)ndshape(src)[2];

  AABBGet(dstbox,&d,0,&shape);
  MatrixXf dst2world(d+1,ndndim(dst)+1);
  dst2world.setIdentity().block<3,3>(0,0).diagonal()
    << (float)shape[0]/(float)ndshape(dst)[0],
       (float)shape[1]/(float)ndshape(dst)[1],
       (float)shape[2]/(float)ndshape(dst)[2];

  Map<Matrix<float,Dynamic,Dynamic,RowMajor> > T(out,ndndim(src)+1,ndndim(dst)+1);
  T=src2world.inverse()*dst2world;
  //  [s,r_s] x [r_d,d]; r_s == r_d
}