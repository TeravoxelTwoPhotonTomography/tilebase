#pragma once
#include <nd.h>
/*
   Tile split for limited memory on the gpu.
   Division is linearly spaced on z.
*/
typedef struct _subdiv_t
{ int i,n,d;      // i is subdiv index, n is number of subdivs, d is dimension of input
  int64_t dz_px,zmax;
  int64_t *dz_nm; // displacement vector for each slice
  float *transform;
  nd_t   carray;
} *subdiv_t;

subdiv_t  make_subdiv(nd_t in, float *transform, int ndim,size_t free,size_t total);

void free_subdiv(subdiv_t ctx);

nd_t subdiv_vol(subdiv_t ctx);

float* subdiv_xform(subdiv_t ctx);

int  next_subdivision(subdiv_t ctx);