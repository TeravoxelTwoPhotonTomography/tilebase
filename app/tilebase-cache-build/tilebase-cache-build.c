#include "tilebase.h"
#include <stdio.h>
#include <string.h>
#include <app/tilebase-cache-build/config.h>

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

// #define PLUGIN_PATH TILEBASE_INSTALL_PATH ## "\\bin\\plugins"

int main(int argc, char *argv[])
{ char *fmt;
  size_t count=0;
  
  ndioAddPluginPath("plugins");
  //ndioAddPluginPath(PLUGIN_PATH); // so installed plugins will be found from the build location  

  if(argc<2)
  { printf("Usage: %s root-path [metadata-format]\n",basename(argv[0]));
    return 0;
  }
  fmt=(argc==3)?argv[2]:0;
  TileBaseClose(TileBaseOpenWithProgressIndicator(argv[1],fmt,progress,(void*)&count));
  printf("\nCached %llu tiles\n",(unsigned long long)count);
  return 0;
}
