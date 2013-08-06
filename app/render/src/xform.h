/**
 * \file
 * Utilities for computing transforms.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#ifndef restrict
  #define restrict __restrict
#endif

#include "nd.h"
#include "src/aabb.h"

void compose(
  float *restrict out,
  aabb_t bbox,
  float sx, float sy, float sz,
  float *restrict transform, unsigned ndim);
void box2box(
  float *restrict out,
  nd_t dst, aabb_t dstbox,
  nd_t src, aabb_t srcbox);

#ifdef __cplusplus
}
#endif