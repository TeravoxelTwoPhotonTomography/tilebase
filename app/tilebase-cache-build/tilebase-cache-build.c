#include "tilebase.h"
#include <stdio.h>

#ifdef _MSC_VER
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

char *basename(char* r)
{ char *o=strrchr(r,PATHSEP);
  return o?(o+1):r;
}

int main(int argc, char *argv[])
{ char *fmt;
  if(argc<2)
      printf("Usage: %s root-path [metadata-format]\n",basename(argv[0]));
  fmt=(argc==3)?argv[2]:0;
  TileBaseClose(TileBaseOpen(argv[1],fmt));
  return 0;
}