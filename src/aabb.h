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

aabb_t AABBCopy(aabb_t dst, aabb_t src);

aabb_t AABBSet(aabb_t self, size_t  ndim, const int64_t*const ori, const int64_t*const shape);
aabb_t AABBGet(aabb_t self, size_t* ndim, int64_t** ori, int64_t** shape);
size_t AABBNDim(aabb_t self);

int    AABBSame(aabb_t a, aabb_t b);

int    AABBHit(aabb_t a, aabb_t b);
aabb_t AABBBinarySubdivision(aabb_t *out, unsigned n, aabb_t a); // n=4 quad, n==8 oct etc...
aabb_t AABBUnionIP(aabb_t dst, aabb_t src);

int64_t AABBVolume(aabb_t self);


#ifdef __cplusplus
}//extern "C"{
#endif
