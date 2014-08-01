#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "nd.h"

//nd_t linspace(nd_t x,float min,float max);
//nd_t normpdf(nd_t x,float mu,float sigma);

/*
Actual sigma used for gaussian is scale * size
If the size is less than size_thresh, returns zero.
*/
nd_t make_aa_filter(float scale,float size, float size_thresh,nd_t workspace);

#ifdef __cplusplus
} //extern "C"
#endif
