/** \file
 *  nD Axis aligned bounding box.
 *
 *  \todo Possible optimizations:  There might be a lot of these objects.  They'll all have the same shape.
 *        It's probably worth trying to reuse objects.  One might have a bunch of objects with the same shape.
 *        Could allocate only unique shapes and then reference those.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "aabb.h"

/// @cond DEFINES
//#define DEBUG
#ifdef DEBUG
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif

#define ENDL                         "\n"
#define LOG(...)                     fprintf(stderr,__VA_ARGS__)
#define TRY(e)                       do{if(!(e)) { LOG("%s(%d): %s"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define NEW(T,e,N)                   TRY((e)=(T*)malloc(sizeof(T)*N))
#define REALLOC(T,e,N)               TRY((e)=(T*)realloc((e),N*sizeof(T)))
#define SAFEFREE(e)                  if(e){free(e); (e)=NULL;}

#define BIT(e,n) (((e)>>(n))&1)

 #ifndef restrict
 #define restrict __restrict
 #endif
/// @endcond

/** Describes an axis aligned bounding box.
 *  Choose fixed precision because floating point sucks.
 *
 *  \todo add a floating point interface that supports a certain precision
 *        e.g. could translate 5.010 to/from 5010.
 */
struct _aabb_t
{ size_t   ndim;
  int64_t *ori;
  int64_t *shape;
};

static
int resize(aabb_t self, size_t ndim)
{ REALLOC(int64_t,self->ori  ,ndim);
  REALLOC(int64_t,self->shape,ndim);
  self->ndim=ndim;
  return 1;
Error:
  return 0;
}

aabb_t AABBMake(size_t ndim)
{ aabb_t out=0;
  NEW(struct _aabb_t,out,1);
  NEW(int64_t,out->ori,ndim);
  NEW(int64_t,out->shape,ndim);
  out->ndim=ndim;
  return out;
Error:
  AABBFree(out);
  return NULL;
}

void AABBFree(aabb_t self)
{ if(!self) return;
  SAFEFREE(self->ori);
  free(self);
}

/**
 * If \a dst is NULL, will construct \a dst before the copy.
 * The caller is responsible for freeing the returned value with AABBFree().
 */
aabb_t AABBCopy(aabb_t dst, aabb_t src)
{ TRY(src);
  if(dst==src) return dst;
  if(!dst) dst=AABBMake(src->ndim);
  TRY(resize(dst,src->ndim));
  memcpy(dst->ori  ,src->ori  ,sizeof(int64_t)*src->ndim);
  memcpy(dst->shape,src->shape,sizeof(int64_t)*src->ndim);
  return dst;
Error:
  return 0;
}

/**
 * Copies \a ori and \a shape to the AABB.
 * Either \a ori or \a shape may be NULL, in which case, the variable won't be copied.
 * If \a self is NULL, a new aabb_t is allocated and returend.  The caller must free.
 */
aabb_t AABBSet(aabb_t self, size_t  ndim, const int64_t* const ori, const int64_t* const shape)
{ if(!self)
    self=AABBMake(ndim);
  TRY(resize(self,ndim));
  if(ori)   memcpy(self->ori,ori,ndim*sizeof(int64_t));
  if(shape) memcpy(self->shape,shape,ndim*sizeof(int64_t));
  return self;
Error:
  return 0;
}

/**
 * Gets the respective pointers for each argument if not NULL.
 * Does not copy.
 */
aabb_t AABBGet(aabb_t self, size_t* ndim, int64_t** ori, int64_t** shape)
{ TRY(self);
#define SET(e,v) if(e) *(e)=(v)
  SET(ndim,self->ndim);
  SET(ori,self->ori);
  SET(shape,self->shape);
#undef SET
  return self;
Error:
  return 0;
}

size_t AABBNDim(aabb_t self)
{ return self?self->ndim:0;
}

static int same(size_t n, int64_t *restrict a, int64_t *restrict b)
{ size_t i;
  for(i=0;i<n;++i) if(a[i]!=b[i]) return 0;
  return 1;
}

int AABBSame(aabb_t a, aabb_t b)
{ size_t i;
  if(a->ndim!=b->ndim) return 0;
  return same(a->ndim,a->ori  ,b->ori) &&
         same(a->ndim,a->shape,b->shape);
}

static unsigned ulog2(unsigned x)
{ unsigned r=0;
  while (x>>=1) r++;
  return r;
}
static unsigned ispow2(unsigned x)
{ return x==(1<<ulog2(x));
}
static void _mn(int64_t *a, int64_t b)
{ *a=(*a<b)?(*a):b;
}
static void _mx(int64_t *a, int64_t b)
{ *a=(*a>b)?(*a):b;
}

/**
 * Intervals are considered closed on the left side and open on the right
 * So that if the maximum of an interval is the same as the minimum of another,
 * they are considered to have zero intersection.
 * \returns 1 if there is a non-empty intersection between \a a and \a b.
 */
int AABBHit(aabb_t a, aabb_t b)
{ int i;
  if(!a || !b) return 0;
  if(a->ndim!=b->ndim) return 0;
  for(i=0;i<a->ndim;++i)
  { int64_t mnmx=a->ori[i]+a->shape[i],
            mxmn=a->ori[i];
    _mn(&mnmx,b->ori[i]+b->shape[i]);
    _mx(&mxmn,b->ori[i]);
    DBG("mnmx: %10d\tmxmn: %10d\n",(int)mnmx,(int)mxmn);
    if(mnmx<=mxmn) return 0;
  }
  DBG("\n");
  return 1;
}

/**
 * Divides \a a into \a n boxes.  The resulting bounding boxes are stored in 
 * \a out.
 *
 * \param[in,out]   out   Must be preallocated with at least \a n elements.
 *                        If <tt>out[i]</tt> is NULL, a new AABB will be allocated,
 *                        otherwise, <tt>out[i]</tt> will be treated as an old AABB
 *                        and be reused.
 * \param[in]       n     Must be a power of two.  \a is divided into \a n 
 *                        subboxes starting on dimension 0 and proceeding
 *                        upwards until n sub-boxes are formed.
 * \param[in]       a     The parent box.  May be a memeber of \a out.
 * \returns \a on success, otherwise 0.
 */
aabb_t AABBBinarySubdivision(aabb_t *out, unsigned n, aabb_t a)
{ unsigned i;
  TRY(ispow2(n));
  TRY(a);
  TRY(out);
  for(i=0;i<n;++i)
    TRY(out[i]=AABBCopy(out[i],a));
  for(i=0;i<n;++i)
  { unsigned d;
    for(d=0;d<a->ndim;++d)
    { if(BIT(n-1,d))   // should subdivide this dim?
      { out[i]->shape[d]/=2;
        if(BIT(i,d)) // should shift this dim?
          out[i]->ori[d]+=out[i]->shape[d];
      }
    }
  }
  return a;
Error:
  return 0;
}

/**
 * Expands \a dst so it contains both \a src and \a dst.
 * If \a dst is NULL, it's treated as an empty box.  A new box is created and
 * returned.  The caller is responsible for freeing the returned box with
 * AABBFree().
 */
aabb_t AABBUnionIP(aabb_t dst, aabb_t src)
{ int i;
  if(!src) return dst;
  if(!dst) TRY(dst=AABBCopy(dst,src));
  TRY(dst->ndim==src->ndim);
  for(i=0;i<dst->ndim;++i)
  { int64_t d=dst->ori[i]+dst->shape[i],
            s=src->ori[i]+src->shape[i],
            r=(d>s)?d:s;
    _mn(dst->ori+i,src->ori[i]);
    dst->shape[i]=r-dst->ori[i];
  }
  return dst;
Error:
  return 0;
}

/**
 * Returns the volume of the bounding box in physical units (nanometers).
 */
int64_t AABBVolume(aabb_t self)
{ int64_t v=1;
  unsigned i;
  for(i=0;i<self->ndim;++i)
    v*=self->shape[i];
  return v;
}
