/**
 * \file
 * Generate a quadtree
 */
#include <stdio.h>
#include <string.h>
#include "tilebase.h"
#include "src/opts.h"
#include "src/address.h"
#include "src/render.h"
#include "src/mkpath.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#define countof(e) (sizeof(e)/sizeof(*e))

#ifdef _MSC_VER
#pragma warning (disable:4996) // deprecation warning for sprintf
#define PATHSEP  '\\'
#else
#define PATHSEP  '/'
#endif
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
{ unsigned k;
  char *op=path;
  for(address=address_begin(address);address&&n>=0;address=address_next(address))
  { k=snprintf(path,n,"%u%c",address_id(address),PATHSEP);
    path+=k;
    n-=k;
  }
  // remove terminal path separator
  if(path!=op) path[-1]='\0';
  return (n>=0)?path:0;
}

unsigned save(nd_t vol, address_t address)
{ char full[1024]={0},
       path[1024]={0};  
  size_t n;
  TRY(address_to_path(path,countof(path),address));
  TRY((n=snprintf(full,countof(full),"%s%c%s",OPTS.dst,PATHSEP,path))>0);
  printf("SAVING %s"ENDL,full);
  TRY(mkpath(full));
  TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,OPTS.dst_pattern))>0);
  ndioClose(ndioWrite(ndioOpen(full,"series","w"),vol));
  return 1;
Error:
  return 0;
}

int main(int argc, char* argv[])
{ unsigned ecode=0; 
  tiles_t tiles=0;
  TRY(parse_args(argc,argv));
  //printf("OPTS: %s %s\n",OPTS.src,OPTS.dst);
  TRY(tiles=TileBaseOpen(OPTS.src,OPTS.src_format));
  TRY(render(tiles,OPTS.x_um,OPTS.y_um,OPTS.z_um,OPTS.countof_leaf,save));
Finalize:
  TileBaseClose(tiles); 
  LOG("Press <ENTER>"ENDL); getchar();
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}

