typedef struct _opts_t
{ const char * path;
  double  x,y,z,lx,ly,lz;
} opts_t;

opts_t parsargs(int *argc, char** argv[], int *isok);