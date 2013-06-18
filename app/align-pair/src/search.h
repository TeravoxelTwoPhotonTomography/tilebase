#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "tilebase.h"

/* If not NULL, the notunique(ts,n) callback is called to handle the case that
   multiple tiles are found.

   The query string is a regular expression matching part of the tile path.

   Returns 0, or a tile reference when the query matches only a single tile.
*/
tile_t* FindByName(
  tile_t *ts,
  size_t ntiles,
  const char* query,
  void (*notunique)(const char* query,tile_t **ts, int n));

#ifdef __cplusplus
}
#endif