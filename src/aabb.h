/** \file
 * nD Axis aligned bounding box.
 *
 * Requires <stdint.h> and <stdlib.h> to be included before this file.
 *
 * \author Nathan Clack
 * \date   July 2012
 */
#pragma once
#ifdef __cplusplus
extern "C"{
#endif

typedef struct _aabb_t* aabb_t;

aabb_t AABBMake(size_t ndim);
void   AABBFree(aabb_t self);

void   AABBCopy(aabb_t dst, aabb_t src);

void   AABBSet(aabb_t self, size_t  ndim, const int64_t*const ori, const int64_t*const shape);
void   AABBGet(aabb_t self, size_t* ndim, int64_t** ori, int64_t** shape);

int    AABBSame(aabb_t a, aabb_t b);

/// \todo unsigned AABBHit(aabb_t a, aabb_t b);
/// \todo unsigned AABBBinarySubdivision(aabb_t *out, unsigned n, aabb_t a); // n=4 quad, n==8 oct etc...


#ifdef __cplusplus
}//extern "C"{
#endif
