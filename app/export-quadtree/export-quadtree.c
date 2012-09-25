/**
 * \file
 * Generate a quadtree
 */
#include <stdio.h>
#include <string.h>
#include "tilebase.h"
#include "src/opts.h"
#include "src/render.h"
#include "src/mkpath.h"
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#define countof(e) (sizeof(e)/sizeof(*e))

#define PATHSEP  '/'
#define ENDL     "\n"
#define LOG(...) fprintf(stderr,__VA_ARGS__) 
#define TRY(e)   do{if(!(e)) { DBG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)

//#define DEBUG
#ifdef DEBUG
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif

/**
 * Renders address to something like "1/2/0/0/4/"
 * \param[in] path    Buffer into which to render the string.
 * \param[in] n       size of the buffer \a path in bytes.
 * \param[in] address The address to render.
 */
char* address_to_path(char* path, int n, address_t address)
{ unsigned i,k;
  while((address=address_next(address)) && n>=0)
  { k=snprintf(path,n,"%u%c",address_id(address),PATHSEP);
    path+=k;
    n-=k;
  }
  return (n>=0)?path:0;
}

unsigned save(nd_t vol, address_t address)
{ char full[1024]={0},
       path[1024]={0};  
  TRY(mkpath(address_to_path(path,countof(path),address)));
  TRY(snprintf(full,countof(full),"%s%c%s%c%s",
    OPTS.dst,PATHSEP,path,PATHSEP,OPTS.dst_pattern));
  ndioClose(ndioWrite(ndioOpen(full,"series","w"),vol));
  return 1;
Error:
  return 0;
}

int main(int argc, char* argv[])
{ unsigned ecode=0;
  tiles_t tiles=0;
  TRY(parse_args(argc,argv));
  printf("OPTS: %s %s\n",OPTS.src,OPTS.dst);
  TRY(tiles=TileBaseOpen(OPTS.src,OPTS.src_format));
  TRY(render(tiles,OPTS.x_um,OPTS.y_um,OPTS.z_um,OPTS.countof_leaf,save));
Finalize:
  TileBaseClose(tiles);
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}

