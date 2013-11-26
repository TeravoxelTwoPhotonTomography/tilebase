#include "config.h" // defines (or not) HAVE_CUDA
#include "src/opts.h"
#include "src/filter.h" // defines: int filters(nd_t hs[3],float scale).  Caller is responsible for freeing the returned nd_t objects in hs.
#include <nd.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG(...)          printf(__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)

#define countof(e) (sizeof(e)/sizeof(*e))

#if 0
#define DUMP(name,a) ndioClose(ndioWrite(dst=ndioOpen((name),NULL,"w"),(a)))
#else
#define DUMP(name,a)
#endif

typedef size_t list_node_t;

typedef struct point {
  size_t   r;
  float    v;
} point_t;

struct store {
  point_t *points;
  size_t sz,cap;
} store = {0};

point_t* new_point(size_t r, float v) {
  point_t *s;
  if(store.sz>=store.cap)
    store.points=(point_t*)realloc(store.points,sizeof(point_t)*(store.cap=(1.2*store.sz+50)));
  if(!store.points) return 0;
  s=store.points+store.sz++;
  s->r=r;
  s->v=v;
  return s;
}

static void ind2sub(nd_t v,size_t idx,float *px,float *py,float *pz) {
  *pz=idx/ndstrides(v)[2];
  idx%=ndstrides(v)[2];
  *py=idx/ndstrides(v)[1];
  idx%=ndstrides(v)[1];
  *px=idx/ndstrides(v)[0];
  return;
}

static int work(opts_t *opts) {
  int  isok=1;
  nd_t a=0,
       b=0, // holds the first filter result
       c=0, // hold dog result
       hs[3]={0}, // filter for each dimension (3)
       ws[2]={0}, // temp space for the convolution.
       out=0;

  ndio_t src=0,dst=0;
  size_t i;
  const nd_conv_params_t params={nd_boundary_replicate};

  /*         */
  /* L O A D */
  /*         */

  TRY( src=ndioOpen(opts->src,NULL,"r"));
  TRY( a=ndheap_ip(ndioShape(src)));
  TRY(ndioRead(src,a));
  DUMP("in.tif",a);
  TRY( ndconvert_ip(a,nd_f32));
  DUMP("in_f32.tif",a);
  /* presumably, can make "a" a gpu array at this point, and the rest of this will be gpu-based. */

  /*       */
  /* D O G */
  /*       */

  // init workspace
  for(i=0;i<countof(ws);++i)
    TRY( ws[i]=ndmake(a));
  TRY(b=ndmake(a));

  // r1
  TRY( ndcopy(ws[0],a,0,0));
  TRY( filters(hs,opts->r1));
  for(i=0;i<3;++i)
    TRY(ndconv1(ws[~i&1],ws[i&1],hs[i],i,&params));
  TRY( ndcopy(b,ws[i&1],0,0));
  for(i=0;i<countof(hs);++i) {
    ndfree(hs[i]);
    hs[i]=0;
  }
  DUMP("r1.tif",b);

  // r2
  TRY( filters(hs,opts->r2));
  TRY( ndcopy(ws[0],a,0,0));
  for(i=0;i<3;++i)
    TRY(ndconv1(ws[~i&1],ws[i&1],hs[i],i,&params));
  TRY( ndcopy(a,ws[i&1],0,0));
  for(i=0;i<countof(hs);++i) {
    ndfree(hs[i]);
    hs[i]=0;
  }
  DUMP("r2.tif",a);

  // r1-r2
  c=ndmake(a);
  TRY( ndfmad_scalar_ip(a,-1.0,0.0,0,0)); // a <= -a
  TRY( ndadd(c,b,a,0,0));                 // c <= b+a
  DUMP("dog.tif",c);

  /*               */
  /* S E G M E N T */
  /*               */

  // Record local maxima above threshold.

  TRY( ndkind(c)==nd_heap ); // require
  { float *v=nddata(c),*vv,*end=v+ndnelem(c);
    list_node_t stack=-1;
#if 0
    const int64_t offsets[]={
      -ndstrides(c)[0],ndstrides(c)[0],
      -ndstrides(c)[1],ndstrides(c)[1],
      -ndstrides(c)[2],ndstrides(c)[2]}; // 6-conn
#else
    const int64_t offsets[]={
      (-ndstrides(c)[0]-ndstrides(c)[1]-ndstrides(c)[2]) /ndstrides(c)[0],
      (-ndstrides(c)[0]-ndstrides(c)[1])                 /ndstrides(c)[0],
      (-ndstrides(c)[0]-ndstrides(c)[1]+ndstrides(c)[2]) /ndstrides(c)[0],
      (-ndstrides(c)[0]                -ndstrides(c)[2]) /ndstrides(c)[0],
      (-ndstrides(c)[0]                )                 /ndstrides(c)[0],
      (-ndstrides(c)[0]                +ndstrides(c)[2]) /ndstrides(c)[0],
      (-ndstrides(c)[0]+ndstrides(c)[1]-ndstrides(c)[2]) /ndstrides(c)[0],
      (-ndstrides(c)[0]+ndstrides(c)[1])                 /ndstrides(c)[0],
      (-ndstrides(c)[0]+ndstrides(c)[1]+ndstrides(c)[2]) /ndstrides(c)[0],
      (                -ndstrides(c)[1]-ndstrides(c)[2]) /ndstrides(c)[0],
      (                -ndstrides(c)[1])                 /ndstrides(c)[0],
      (                -ndstrides(c)[1]+ndstrides(c)[2]) /ndstrides(c)[0],
      (                                -ndstrides(c)[2]) /ndstrides(c)[0],
      (                                 ndstrides(c)[2]) /ndstrides(c)[0],
      (                 ndstrides(c)[1]-ndstrides(c)[2]) /ndstrides(c)[0],
      (                 ndstrides(c)[1])                 /ndstrides(c)[0],
      (                 ndstrides(c)[1]+ndstrides(c)[2]) /ndstrides(c)[0],
      ( ndstrides(c)[0]-ndstrides(c)[1]-ndstrides(c)[2]) /ndstrides(c)[0],
      ( ndstrides(c)[0]-ndstrides(c)[1])                 /ndstrides(c)[0],
      ( ndstrides(c)[0]-ndstrides(c)[1]+ndstrides(c)[2]) /ndstrides(c)[0],
      ( ndstrides(c)[0]                -ndstrides(c)[2]) /ndstrides(c)[0],
      ( ndstrides(c)[0]                )                 /ndstrides(c)[0],
      ( ndstrides(c)[0]                +ndstrides(c)[2]) /ndstrides(c)[0],
      ( ndstrides(c)[0]+ndstrides(c)[1]-ndstrides(c)[2]) /ndstrides(c)[0],
      ( ndstrides(c)[0]+ndstrides(c)[1])                 /ndstrides(c)[0],
      ( ndstrides(c)[0]+ndstrides(c)[1]+ndstrides(c)[2]) /ndstrides(c)[0],
    }; // 26-conn
#endif
    for(vv=v;vv<end;++vv) {
      const float t=vv[0];
      int64_t *o;
      if(t>opts->thresh) {
        for(i=0,o=offsets;i<countof(offsets);++i,++o)
          if( v<=(vv+o[0]) && (vv+o[0])<end && // rough boundary check - this doesn't really properly handle boundaries
              vv[o[0]]>t ) break;                    // ensure current value is greater than neighbors
        if(i==countof(offsets))                            // record a local max
          TRY( new_point((vv-v)*sizeof(float),t));
      }
    }

    /*             */
    /* O U T P U T */
    /*             */
    out=ndreshapev(ndcast(ndinit(),nd_f32),2,4,store.sz); // will store x,y,z,intensity
    TRY(out=ndref(out,malloc(ndnbytes(out)),nd_heap));
    { float *o=nddata(out);
      for(i=0;i<store.sz;++i,o+=4) {
        ind2sub(c,store.points[i].r,o,o+1,o+2);
        o[3]=store.points[i].v;
      }
    }
    TRY(ndioWrite(dst=ndioOpen(opts->dst,"hdf5","w"),out));
  }


Finalize:
  ndioClose(src);
  ndioClose(dst);
  ndfree(a);
  ndfree(b);
  ndfree(c);
  ndfree(out);
  for(i=0;i<countof(hs);++i)
    ndfree(hs[i]);
  for(i=0;i<countof(ws);++i)
    ndfree(ws[i]);
  return isok;
Error:
  isok=0;
  goto Finalize;
}

int main(int argc,char* argv[]) {
  int isok=1;
  opts_t opts={0};
  opts=parseargs(&argc,&argv,&isok);
  if(!isok) goto Error;
  TRY(work(&opts));
  return 0;
Error:
  return 1;
}
