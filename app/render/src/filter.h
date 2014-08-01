#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "nd.h"

//nd_t linspace(nd_t x,float min,float max);
//nd_t normpdf(nd_t x,float mu,float sigma);
nd_t make_aa_filter(float scale,float scale_thresh,nd_t workspace);

#ifdef __cplusplus
} //extern "C"
#endif
