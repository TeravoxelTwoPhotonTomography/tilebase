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
#include "src/fixup.h"

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

#define max(a,b)  (((a)<(b))?(b):(a))

int g_flag_loaded_from_tree=0;

static unsigned print_addr(nd_t v, address_t address, void* args)
{ FILE* fp=args;
  char path[1024]={0},*t;
#ifdef FANCY
  fprintf(fp,"-- %-10d  %s"ENDL,
    (int)address_to_int(address,10),
    (t=address_to_path(path,countof(path),address))?t:"(null)");
#else
  fprintf(fp,"%u"ENDL,(unsigned)address_to_int(address,10));
#endif
  return 0;
}

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


unsigned save_raveler(nd_t vol, address_t address, void* args)
{ char full[1024]={0},
       path[1024]={0};  
  size_t n;
  nd_t tmp=0,tmp2=0;
  int isok=1;
  
  TRY(ndcopy(tmp=ndheap(vol),vol,0,0));
  TRY(ndShapeSet(tmp,3,1));                                   // select first channel

  if(!g_flag_loaded_from_tree)
  { TRY(ndLinearConstrastAdjust_ip(tmp,nd_u8,-24068,-14428));   // scale it
    TRY(ndsaturate_ip(tmp,0,255));
  }
  TRY(tmp2=ndcast(ndheap(tmp),nd_u8));
  TRY(ndcopy(tmp2,tmp,0,0));

  TRY(address_to_path(path,countof(path),address));
  TRY((n=snprintf(full,countof(full),"%s%c%s",OPTS.dst,PATHSEP,path))>0);
  printf("SAVING %s"ENDL,full);
  TRY(mkpath(full));
  TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,OPTS.dst_pattern))>0);
  ndioClose(ndioWrite(ndioOpen(full,NULL,"w"),tmp2));
Finalize:
  ndfree(tmp);
  ndfree(tmp2);
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
  g_flag_loaded_from_tree=1;
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


uint64_t nextpow2(uint64_t v)
{ v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  return ++v;
}

int main(int argc, char* argv[])
{ unsigned ecode=0; 
  tiles_t tiles=0;
  handler_t on_ready=save;
  TRY(parse_args(argc,argv));
  cudaSetDevice(OPTS.gpu_id);
  //printf("OPTS: %s %s\n",OPTS.src,OPTS.dst);
  TRY(tiles=TileBaseOpen(OPTS.src,OPTS.src_format));
  TRY(fix_fov(tiles,OPTS.fov_x_um*1000.0,OPTS.fov_y_um*1000.0));

  if(OPTS.flag_raveler_output)
  { aabb_t bbox;
    int64_t *shape=0,s;
    uint64_t v;
    double   f;
    on_ready=save; //save_raveler;
    DBG("--- BEFORE\n");
    TRY(bbox=AdjustTilesBoundingBox(tiles,&OPTS.ox,&OPTS.lx));
    TRY(AABBGet(bbox,0,0,&shape));
    DBG("NANOMETERS (%15lld,%15lld,%15lld)\n",(long long)shape[0],(long long)shape[1],(long long)shape[2]);
    DBG("PIXELS     (%15f,%15f,%15f)\n",((double)shape[0])/1000.0/OPTS.x_um,
                                           ((double)shape[1])/1000.0/OPTS.y_um,
                                           ((double)shape[2])/1000.0/OPTS.z_um);
    // expand bounds to next power of 2 in pixels along the x and y dimension.  Make the x and y extent. the same number of pixels.
    s=max(shape[0],shape[1]);
    TRY(OPTS.x_um==OPTS.y_um);
    v=nextpow2((double)s/1000.0/OPTS.x_um); // Assumes x and y have same output resolution
    f=v/((double)shape[0]/1000.0/OPTS.x_um);
    OPTS.lx*=f;           // scale
    OPTS.ox+=0.5*(1.0-f); // center up
    f=v/((double)shape[1]/1000.0/OPTS.y_um);
    OPTS.ly*=f;
    OPTS.oy+=0.5*(1.0-f);
    AABBFree(bbox);
    TRY(bbox=AdjustTilesBoundingBox(tiles,&OPTS.ox,&OPTS.lx));
    TRY(AABBGet(bbox,0,0,&shape));
    DBG("--- AFTER\n");
    DBG("NANOMETERS (%15lld,%15lld,%15lld)\n",(long long)shape[0],(long long)shape[1],(long long)shape[2]);
    DBG("PIXELS     (%15f,%15f,%15f)\n",((double)shape[0])/1000.0/OPTS.x_um,
                                           ((double)shape[1])/1000.0/OPTS.y_um,
                                           ((double)shape[2])/1000.0/OPTS.z_um);
    AABBFree(bbox);
  }

  if(OPTS.flag_print_addresses)
  { TRY(addresses(tiles,&OPTS.x_um,&OPTS.ox,&OPTS.lx,OPTS.nchildren,OPTS.countof_leaf,print_addr,stdout));
    goto Finalize;
  }

  if(OPTS.target)
  { printf("RENDERING TARGET: ");
    print_addr(0,OPTS.target,stdout);
    TRY(render_target(tiles,&OPTS.x_um,&OPTS.ox,&OPTS.lx,OPTS.nchildren,OPTS.countof_leaf,on_ready,NULL,load,OPTS.target));
    goto Finalize;
  }
  
  TRY(render(tiles,&OPTS.x_um,&OPTS.ox,&OPTS.lx,OPTS.nchildren,OPTS.countof_leaf,on_ready,NULL));
  
Finalize:
  TileBaseClose(tiles); 
#ifdef _MSC_VER //helps with msvc debugging
  LOG("Press <ENTER>"ENDL); getchar();
#endif
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}

