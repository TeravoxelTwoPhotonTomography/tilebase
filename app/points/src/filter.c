/**
 * Most of this was identical to app/render's aa filter.
 */

#include "filter.h"

#include <stdio.h>
#include <math.h>
#include "nd.h"
#ifdef _MSC_VER
#define isfinite _finite
#endif

#define ENDL        "\n"
#define LOG(...)    fprintf(stderr,__VA_ARGS__) 
#define TRY(e)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)

#define restrict __restrict

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
  float *restrict d=(float*)nddata(x);
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
{ float norm=0.0;
  float*restrict d=(float*)nddata(x);
  size_t i,n=ndnelem(x);
  TRY(ndtype(x)==nd_f32);
  for(i=0;i<n;++i)
  { float v=(d[i]-mu)/sigma;
    d[i]=exp(-0.5*v*v);
  }
  for(i=0;i<n;++i) norm+=d[i];
  for(i=0;i<n;++i) d[i]/=norm;
  return x;
Error:
  return 0;
}

/** single valued normal probability desnity */
static float _normpdf(float v, float mu, float sigma)
{ const float norm=1.0/sigma/SQRT_2PI;
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


static nd_t make_filter(float scale,nd_t workspace)
{ 
  nd_t out=workspace;  
  size_t r,n; //filter radius, size
  scale=fabs(scale)/2.0; // this is the sigma that gets used...the divisor is pretty arbitrary
  //if(scale<=1.0f) return 0; // no filter needed
  r=gaussian_filter_radius(scale,0.01);
  n=2*r+1;
  if(!out)
    TRY(ndreshapev(ndcast(ndref(out=ndinit(),malloc(n*sizeof(float)),nd_heap),nd_f32),1,n));
  if(ndnelem(out)<n)
    TRY(ndreshapev(ndref(out,realloc(nddata(out),n*sizeof(float)),nd_heap),1,n));
  linspace(out,-(float)r,r);
  normpdf(out,0.0f,scale);
#if 0
  ndioClose(ndioWrite(ndioOpen("filter.h5",0,"w"),out));
#endif
  return out;
Error:
  return 0;
}

//  Caller is responsible for freeing the returned nd_t objects in hs.
int filters(nd_t hs[3],float scale) {
  int i;
  for(i=0;i<3;++i)
    TRY(hs[i]=make_filter(scale,0));
  return 1;
Error:
  return 0;
}

