#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <nd.h>

int filters(nd_t hs[3],float scale); //  Caller is responsible for freeing the returned nd_t objects in hs.

#ifdef __cplusplus
} //extern "C" {
#endif

