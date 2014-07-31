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
#include "src/fixup.h"
#include <app/render/config.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#if HAVE_CUDA
#else
#define cudaSetDevice(...)
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

#define DEBUG
#ifdef DEBUG
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif

#undef max
#define max(a,b)  (((a)<(b))?(b):(a))

int g_flag_loaded_from_tree=0;
opts_t OPTS={0};

/** Adjusts OPTS so the bounding box matches the address.
 *
 *  Starting OPTS.ox and OPTS.lx define the domain over which the addresses
 *  are intended to operate.  The target address gives some subdivision
 *  within that box.  This function adjusts the box in place.
 *
 *  Also @see Note[1] at bottom of this file.
 *
 *  @returns 1 on success, 0 otherwise
 */
static void target_bbox(tiles_t tiles) {
#define BIT(x,i) ((x>>i)&1)
  int i;
  address_t a = OPTS.target;
  double *os=&OPTS.ox,
         *ls=&OPTS.lx;
  if(!a) return;
  for(a=address_begin(a);a;a=address_next(a)) {
    const unsigned id=address_id(a);
    for(i=0;i<3;++i) {
      ls[i]/=2.0;
      os[i]+=BIT(id,i)*ls[i];
    }
  }
#undef BIT
}

static unsigned print_addr(nd_t v, address_t address, aabb_t bbox, void* args)
{ FILE* fp=(FILE*)args;
  char path[1024]={0};
  if(OPTS.target && OPTS.flag_print_addresses) {
    unsigned id=(unsigned)address_to_int(address,10);
    fprintf(fp,id?"%u%u"ENDL:"%u"ENDL,(unsigned)address_to_int(OPTS.target,10),id);
  } else {
    fprintf(fp,"%u"ENDL,(unsigned)address_to_int(address,10));
  }
  return 0;
}

static unsigned save(nd_t vol, address_t address, aabb_t bbox, void* args)
{ char full[1024]={0},
       path[1024]={0};
  size_t n;
  FILE *fp=0; // for transform.txt
  //nd_t tmp=0;
  int isok=1;
  //TRY(ndcopy(tmp=ndheap(vol),vol,0,0));
  //TRY(ndconvert_ip(tmp,nd_u16));

  TRY(address_to_path(path,countof(path),address));
  TRY((n=snprintf(full,countof(full),"%s%c%s",OPTS.dst,PATHSEP,path))>0);
  printf("SAVING %s"ENDL,full);
  TRY(mkpath(full));
  TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,OPTS.dst_pattern))>0);
  ndioClose(ndioWrite(ndioOpen(full,ndioFormat("series"),"w"),vol));

  // output text document with origin and scale
  if(bbox)
  { int64_t *o,*s;
    int i;
    char *lbls[] = {"ox","oy","oz","sx","sy","sz"};

    memset(full+n,0,countof(full)-n);
    TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,"transform.txt"))>0);
    TRY(fp=fopen(full,"w"));

    TRY(AABBGet(bbox,0,&o,&s));
    for(i=0;i<3;++i) // translation
      fprintf(fp,"%s: %lld\n",lbls[i],(long long)o[i]);
    for(i=0;i<3;++i) // scale:: r_nm = s * r_px
      fprintf(fp,"%s: %f\n",lbls[i+3],(double)s[i]/(double)ndshape(vol)[i]);
    fclose(fp); fp=0;
  }

  // optionally output orthogonal views
  if(OPTS.flag_output_ortho) {
    nd_t xy=0,yz=0,zx=0;
    unsigned d;
    TRY(yz=ndheap(vol));
    TRY(zx=ndheap(vol));
    TRY(xy=ndcopy(ndheap(vol),vol,0,0)); // force copy to heap; strided copy from gpu doesn't work yet
    d=ndndim(vol);
    // set everyone to have 3 dims temporarily
    ndreshape(xy ,3,ndshape(xy));
    ndreshape(yz ,3,ndshape(yz));
    ndreshape(zx ,3,ndshape(zx));

    // permute dimensions (copy)
    TRY(ndshiftdim(zx,xy,1));
    TRY(ndshiftdim(yz,xy,2));

    // reset everyone to original dimensionality
    ndreshape(xy ,d,ndshape(xy));
    ndreshape(yz ,d,ndshape(yz));
    ndreshape(zx ,d,ndshape(zx));

    // output
    memset(full+n,0,countof(full)-n);
    TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,"YZ.%.tif"))>0);
    ndioClose(ndioWrite(ndioOpen(full,ndioFormat("series"),"w"),yz));
    ndfree(yz);

    memset(full+n,0,countof(full)-n);
    TRY((snprintf(full+n,countof(full)-n,"%c%s",PATHSEP,"ZX.%.tif"))>0);
    ndioClose(ndioWrite(ndioOpen(full,ndioFormat("series"),"w"),zx));
    ndfree(zx);
  }
Finalize:
  //ndfree(tmp);
  if(fp) fclose(fp);
  return isok;
Error:
  isok=0;
  goto Finalize;
}


static unsigned save_raveler(nd_t vol, address_t address, aabb_t bbox, void* args)
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
  ndioClose(ndioWrite(ndioOpen(full,ndioFormat("series"),"w"),tmp2));
Finalize:
  ndfree(tmp);
  ndfree(tmp2);
  return isok;
Error:
  isok=0;
  goto Finalize;
}

static nd_t load(address_t address)
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
  TRY(f=ndioOpen(full,0,"r"));
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


static uint64_t nextpow2(uint64_t v)
{ v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  return ++v;
}

static void set_render_opts(struct render* opts)
{
#define CPY(d,s) memcpy(opts->d,&OPTS.s,sizeof(opts->d));
  CPY(voxel_um,x_um);
  CPY(ori     ,ox);
  CPY(size    ,lx);
#undef CPY
  opts->countof_leaf=OPTS.countof_leaf;
  opts->nchildren   =OPTS.nchildren;
}


int main(int argc, char* argv[])
{ unsigned ecode=0;
  tiles_t tiles=0;
  handler_t on_ready=save;
  struct render render_opts={0};
  ndioAddPluginPath("plugins");             // search this path for plugins (relative to executable)
  ndioAddPluginPath(TILEBASE_INSTALL_PATH); // so installed plugins will be found from the build location
  { int isok=0;
    OPTS=parseargs(&argc,&argv,&isok);
    TRY(isok);
  }
  set_render_opts(&render_opts);
  fprintf(stderr,"GPU: %d\n",(int)(OPTS.gpu_id));
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
  { target_bbox(tiles);
    TRY(addresses(&render_opts,tiles,print_addr,stdout));
    goto Finalize;
  }

  if(OPTS.target)
  { printf("RENDERING TARGET: ");
    print_addr(0,OPTS.target,0,stdout);
    TRY(render_target(&render_opts,tiles,on_ready,NULL,load,OPTS.target));
    goto Finalize;
  }

  TRY(render(&render_opts,tiles,on_ready,NULL));

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

/** Notes
 *
 *  [1]: target_bbox
 *       The goal here was to restrict --print-address output to the domain
 *       of the --target-address.  A simpler approach would've been to just
 *       filter printed addresses such that the target address was a required
 *       prefix.  I didn't realize this until I already had the current
 *       implementation done, so the "simpler" route will not be taken; maybe
 *       it's debatable which is simpler.
 */
