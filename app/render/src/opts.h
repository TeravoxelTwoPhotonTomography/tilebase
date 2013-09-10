/**
 * \file
 * Program options.
 *
 * Requires address.h included before this file.
 */
#pragma once
#ifdef __cplusplus
 extern "C" {
#endif

#include "address.h"

typedef struct _opts_t
{ const char *src;
  const char *dst;
  const char *src_format;
  const char *dst_pattern;

  double x_um;
  double y_um;
  double z_um;

  double ox,oy,oz;
  double lx,ly,lz;

  unsigned nchildren;
  unsigned countof_leaf;

  unsigned  flag_print_addresses;
  unsigned  flag_raveler_output;

  double    fov_x_um; // if <0 use value from tile database, otherwise force fov size on tiles.
  double    fov_y_um;

#if 0  // TODO
  unsigned  flag_adjust_contrast;
  double    contrast_min;
  double    contrast_max;
#endif

  address_t target; // if not NULL, will try to render target from it's children.
  int gpu_id;
} opts_t;

opts_t parseargs(int *argc, char** argv[], int *isok);

#ifdef __cplusplus
} //extern "C" {
#endif
