#include "tilebase.h"
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

char *basename(char* r)
{ char *o=strrchr(r,PATHSEP);
  return o?(o+1):r;
}

void progress(const char *path, void* data)
{ size_t* c=(size_t*)data;
  ++*c;
  printf("."); fflush(stdout);
}

int main(int argc, char *argv[])
{ char *fmt;
  size_t count=0;
  
  ndioAddPluginPath("plugins");
  if(argc<2)
  { printf("Usage: %s root-path [metadata-format]\n",basename(argv[0]));
    return 0;
  }
  fmt=(argc==3)?argv[2]:0;
  TileBaseClose(TileBaseOpenWithProgressIndicator(argv[1],fmt,progress,(void*)&count));
  printf("\nCached %llu tiles\n",(unsigned long long)count);
  return 0;
}
