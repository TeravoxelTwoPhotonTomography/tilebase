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

typedef struct _opts_t
{ const char *src;
  const char *dst;
  const char *src_format;
  const char *dst_pattern;

  float x_um;
  float y_um;
  float z_um;

  float ox,oy,oz;
  float lx,ly,lz;

  unsigned nchildren;
  unsigned countof_leaf;

  unsigned  flag_print_addresses;
  address_t target; // if not NULL, will try to render target from it's children.
  int gpu_id;
} opts_t;

extern opts_t OPTS; // Global variable for holding config loaded from parse_args

/**
 * Fills in the opts_t struct, which contains options read from the command
 * line (or possibly a configuration file).  Should only be called once at the
 * beginning of a program.
 *
 * \returns 1 on sucess, 0 otherwise
 */
unsigned parse_args(int argc, char *argv[]);


#ifdef __cplusplus
} //extern "C" {
#endif