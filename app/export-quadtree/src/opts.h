/**
 * \file
 * Program options
 */
#pragma once
#ifdef __cplusplus
 extern "C" {
#endif

typedef struct _opts_t
{
  const char *src;
  const char *dst;
  const char *src_format;
  const char *dst_pattern;

  float x_um;
  float y_um;
  float z_um;

  unsigned countof_leaf;
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