typedef struct _opts_t
{ const char * path;
  const char * query;
  int ins,del,subst,max; // cost structure for approximate matching
} opts_t;

opts_t parsargs(int *argc, char** argv[], int *isok);