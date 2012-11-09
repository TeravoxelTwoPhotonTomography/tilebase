/**
 * \file
 * Generate a quadtree
 */
#include <stdio.h>
#include <string.h>
#include "tilebase.h"
#include "src/address.h"
#include "src/render.h"
#include "src/mkpath.h"
#include "src/opts.h"

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

unsigned save(nd_t vol, address_t address, void* args)
{ char full[1024]={0},
       path[1024]={0};  
  size_t n;
  //nd_t tmp=0;
  int isok=1;
  //TRY(ndcopy(tmp=ndheap(vol),vol,0,0));
  //TRY(ndconvert_ip(tmp,nd_u16));

  TRY(address_to_path(path,countof(path),address));
  TRY((n=snprintf(full,countof(full),"%s%c%s",OPTS.dst,PATHSEP,path))>0);
  printf("SAVING %s"ENDL,full);
  TRY(mkpath(full));
  TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,OPTS.dst_pattern))>0);
  ndioClose(ndioWrite(ndioOpen(full,NULL,"w"),vol));
Finalize:
  //ndfree(tmp);
  return isok;
Error:
  isok=0;
  goto Finalize;
}

nd_t load(address_t address)
{ char full[1024]={0},
       path[1024]={0};  
  size_t n;
  nd_t out=0;
  ndio_t f=0;
  TRY(address_to_path(path,countof(path),address));
  TRY((n=snprintf(full,countof(full),"%s%c%s",OPTS.dst,PATHSEP,path))>0);  
  TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,OPTS.dst_pattern))>0);
  printf("LOADING %s"ENDL,full);
#if 1
  TRY(f=ndioOpen(full,NULL,"r"));
  TRY(out=ndioShape(f));
  TRY(ndref(out,malloc(ndnbytes(out)),nd_heap));
  TRY(ndioRead(f,out));
#endif
Finalize:
  ndioClose(f);
  return out;
Error:
  ndfree(out); out=0;
  goto Finalize;
}

static unsigned print_addr(nd_t v, address_t address, void* args)
{ FILE* fp=args;
  char path[1024]={0},*t;
  fprintf(fp,"-- %-5d  %s"ENDL,
    (int)address_to_int(address,10),
    (t=address_to_path(path,countof(path),address))?t:"(null)");
  return 0;
}

int main(int argc, char* argv[])
{ unsigned ecode=0; 
  tiles_t tiles=0;
  TRY(parse_args(argc,argv));
  cudaSetDevice(OPTS.gpu_id);
  //printf("OPTS: %s %s\n",OPTS.src,OPTS.dst);
  TRY(tiles=TileBaseOpen(OPTS.src,OPTS.src_format));

  if(OPTS.flag_print_addresses)
  { TRY(addresses(tiles,&OPTS.x_um,&OPTS.ox,&OPTS.lx,OPTS.countof_leaf,print_addr,stdout));
    goto Finalize;
  }

  if(OPTS.target)
  { printf("RENDERING TARGET: ");
    print_addr(0,OPTS.target,stdout);
    TRY(render_target(tiles,&OPTS.x_um,&OPTS.ox,&OPTS.lx,OPTS.countof_leaf,save,NULL,load,OPTS.target));
    goto Finalize;
  }
  
  TRY(render(tiles,&OPTS.x_um,&OPTS.ox,&OPTS.lx,OPTS.countof_leaf,save,NULL));
  
Finalize:
  TileBaseClose(tiles); 
  LOG("Press <ENTER>"ENDL); getchar();
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}

