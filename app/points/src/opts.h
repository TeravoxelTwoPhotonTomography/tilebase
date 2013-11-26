typedef struct _opts_t
{ const char *src,*dst;
  double r1,r2,thresh;
} opts_t;

opts_t parseargs(int *argc, char** argv[], int *isok);
