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
#define ENDL                         "\n"
#define LOG(...)                     fprintf(stderr,__VA_ARGS__)
#define TRY(e)                       do{if(!(e)) { LOG("%s(%d): %s"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define NEW(T,e,N)                   TRY((e)=(T*)malloc(sizeof(T)*N))
#define REALLOC(T,e,N)               TRY((e)=(T*)realloc((e),N*sizeof(T)))
#define SAFEFREE(e)                  if(e){free(e); (e)=NULL;}
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
{ REALLOC(int64_t,self->ori,2*ndim);
  self->ndim=ndim;
  return 1;
Error:
  return 0;
}

aabb_t AABBMake(size_t ndim)
{ aabb_t out=0;
  NEW(struct _aabb_t,out,1);
  NEW(int64_t,out->ori,2*ndim);
  out->shape=out->ori+ndim;
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

void AABBCopy(aabb_t dst, aabb_t src)
{ resize(dst,src->ndim);
  memcpy(dst->ori,src->ori,sizeof(int64_t)*2*src->ndim);
}

/**
 * Copies \a ori and \a shape to the AABB.
 * Either \a ori or \a shape may be NULL, in which case, it won't be copied.
 */
void AABBSet(aabb_t self, size_t  ndim, const int64_t* const ori, const int64_t* const shape)
{ resize(self,ndim);
  if(ori)   memcpy(self->ori,ori,ndim*sizeof(int64_t));
  if(shape) memcpy(self->shape,shape,ndim*sizeof(int64_t));
}

void AABBGet(aabb_t self, size_t* ndim, int64_t** ori, int64_t** shape)
{
#define SET(e,v) if(e) *(e)=(v)
  SET(ndim,self->ndim);
  SET(ori,self->ori);
  SET(shape,self->shape);
#undef SET
}

int AABBSame(aabb_t a, aabb_t b)
{ size_t i;
  if(a->ndim!=b->ndim) return 0;
  for(i=0;i<2*a->ndim;++i)
    if(a->ori[i]!=b->ori[i])
      return 0;
  return 1;
}
