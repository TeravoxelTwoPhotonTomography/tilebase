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
} opts_t;

/**
 * Fills in the opts_t struct, which contains options read from the command
 * line (or possibly a configuration file).  Should only be called once at the
 * beginning of a program.
 *
 * \returns 1 on sucess, 0 otherwise
 */
unsigned parse_args(opts_t *opts, int argc, char *argv[]);



#ifdef __cplusplus
} //extern "C" {
#endif