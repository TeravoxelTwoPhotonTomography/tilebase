/**
 * \file
 * Antialiasing filters
 */

#include <stdio.h>
#include <math.h>
#include "nd.h"

#define ENDL        "\n"
#define LOG(...)    fprintf(stderr,__VA_ARGS__) 
#define TRY(e)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)

#define restrict __restrict__

#define SQRT_2PI (2.5066282746310002)

/**
 * Fills \a x with ndnelem(x) evenly spaced numbers from \a min to \a max 
 * (inclusive).
 * \param[in,out] x A preallocated float nd_t array.
 * \returns \a x on success, otherwise 0
 */
static nd_t linspace(nd_t x, float min, float max)
{ size_t i,n=ndnelem(x);
  const float dx=(max-min)/(n-1);
  float *restrict d=nddata(x);
  TRY(ndtype(x)==nd_f32);
  for(i=0;i<n;++i)
    d[i]=min+i*dx;
  return x;
Error:
  return 0;
}

/**
 * Replaces each value \a v in \a x, with the normal probability density 
 * at \a v.
 */
static nd_t normpdf(nd_t x,float mu, float sigma)
{ const float norm=1.0/sigma*SQRT_2PI;
  float*restrict d=nddata(x);
  size_t i,n=ndnelem(x);
  TRY(ndtype(x)==nd_f32);
  for(i=0;i<n;++i)
  { float v=(d[i]-mu)/sigma;
    d[i]=norm*exp(-0.5*v*v);
  }
  return x;
Error:
  return 0;
}

/** single valued normal probability desnity */
static float _normpdf(float v, float mu, float sigma)
{ const float norm=1.0/sigma*SQRT_2PI;
  v=(v-mu)/sigma;
  return norm*exp(-0.5*v*v);
}
/** map density to (positive) x value*/
static float _invnorm(float v, float mu, float sigma)
{ const float norm=sigma*SQRT_2PI;
  return sqrt(-2.0*sigma*sigma*log(norm*v))+mu;
}
/** Calculate the filter radius for a gaussian such that densities of at 
    least \a threshold are in the support.
  */
static size_t gaussian_filter_radius(float sigma, float threshold)
{ return ceil(_invnorm(threshold,0,sigma));
}

nd_t make_aa_filter(float scale,nd_t workspace)
{ nd_t out=workspace;  
  size_t r,n; //filter radius, size
  TRY(isfinite(scale));
  scale=fabs(scale);
  //if(scale<=1.0f) return 0; // no filter needed
  r=gaussian_filter_radius(scale,0.01);
  n=2*r+1;
  if(!out)
    TRY(ndcast(ndref(out=ndinit(),malloc(n*sizeof(float)),n),nd_f32));
  if(ndnelem(out)<n)
    TRY(ndref(out,realloc(nddata(out),n*sizeof(float)),n));
  linspace(out,-(float)r,r);
  normpdf(out,0.0f,scale/2.0);
  return out;
Error:
  return 0;

}