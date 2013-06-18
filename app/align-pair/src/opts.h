#pragma
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _opts_t
{ const char * path;
  const char * query[2];
  const char * output;   // if not null, output selected tiles in a cache file here
  int nocorners;
} opts_t;

opts_t parsargs(int *argc, char** argv[], int *isok);

#ifdef __cplusplus
} //extern "C"
#endif
