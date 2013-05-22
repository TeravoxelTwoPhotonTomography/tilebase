/**
 * query-aabb for tilebase
 * Usage:
 *   query-aabb <path> [-x <x>] [-x <x>] [-x <x>] [-lx <x>] [-ly <x>] [-lz <x>]
 */

#include "tilebase.h"
#include "src/opts.h"
#include <stdio.h>

#define LOG(...)          printf(__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)

aabb_t make_qbox(tiles_t tb,double o[3],double s[3])
{ aabb_t bbox;
  int64_t *ori,*shape;
  int i;
  TRY(bbox=TileBaseAABB(tb));
  AABBGet(bbox,0,&ori,&shape);
  for(i=0;i<3;++i) ori[i]  =ori[i]+o[i]*shape[i];
  for(i=0;i<3;++i) shape[i]=       s[i]*shape[i];
  return bbox;
Error:
  return 0;
}

int main(int argc,char*argv[])
{ opts_t opts;
  int isok,ecode=0;
  tiles_t tb=0;
  aabb_t qbox=0;

  opts=parsargs(&argc,&argv,&isok);
  if(!isok) return 1;

  TRY(tb=TileBaseOpen(opts.path,NULL));
  TRY(qbox=make_qbox(tb,&opts.x,&opts.lx));

  { tile_t *ts=0;
    int i;  
    TRY(ts=TileBaseArray(tb));
    for(i=0;i<TileBaseCount(tb);++i)
    { if(AABBHit(qbox,TileAABB(ts[i])))
        printf("%s\n",TilePath(ts[i]));
    }
  }

Finalize:
  TileBaseClose(tb);
  AABBFree(qbox);
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}