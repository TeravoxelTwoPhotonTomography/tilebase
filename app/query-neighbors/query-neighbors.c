/**
 * query-name for tilebase
 * Usage:
 *   query-name <path> <query>
 */

#include "tilebase.h"
#include "src/cache.h"
#include "src/opts.h"
#include "tre/tre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG(...)          fprintf(stderr,__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)
#define NEW(T,e,N)        TRY((e)=malloc(sizeof(T)*(N)))
#define ZERO(T,e,N)       memset((e),0,sizeof(T)*(N))

#define min(a,b) (a<b?a:b)

struct stack_t 
{ int* data;
  int n,sz;
} stack={0,0,0};

int push(int v)
{
#if 1
  if(stack.n>=(stack.sz-1))
  { stack.sz=1.2*stack.n+50;
    TRY(stack.data=realloc(stack.data,stack.sz*sizeof(int)));
  }
  stack.data[stack.n++]=v;
#endif
  return 1;
Error:
  return 0;
}
void clear() { stack.n=0; }
int pop()   { return (stack.n>0)?(stack.data[--stack.n]):(-1); }


unsigned hit(tile_t *a, void *ctx)
{ aabb_t box=(aabb_t)ctx;
  return AABBHit(TileAABB(*a),box);
}

int main(int argc,char*argv[])
{ opts_t opts;
  int isok,ecode=0;
  tiles_t tb=0;
  regex_t reg={0};
  aabb_t qbox=0;
  regaparams_t params={0};
  regamatch_t  match={0};

  opts=parsargs(&argc,&argv,&isok);
  if(!isok) return 1;

  TRY(0==tre_regcomp(&reg,opts.query,REG_NOSUB));
  params.cost_ins   =opts.ins;
  params.cost_del   =opts.del;
  params.cost_subst =opts.subst;
  params.max_cost   =opts.max;
  params.max_ins    =opts.max;
  params.max_del    =opts.max;
  params.max_subst  =opts.max;
  params.max_err    =opts.max;

  TRY(tb=TileBaseOpen(opts.path,NULL));
  // Find matching names
  { tile_t *ts=0;
    int i;
    int min=99999;
    TRY(ts=TileBaseArray(tb));
    for(i=0;i<TileBaseCount(tb);++i)
    { if(0==tre_regaexec(&reg,TilePath(ts[i]),&match,params,0))
      { if(match.cost<min)
        { clear();
          min=match.cost;
        }
        if(match.cost==min)
          push(i);
      }
    }
    TRY((i=pop())>=0);
    TRY(qbox=TileAABB(ts[i]));
    fprintf(stderr,"Looking for neighbors of:\n\t%s\n",TilePath(ts[i]));
  }
  if(opts.output)
  { tile_t *ts=0;
    char *root=0;
    tilebase_cache_t out=0;
    size_t nout=0;
    TRY(ts=TilesFilter(TileBaseArray(tb),TileBaseCount(tb),&nout,hit,qbox));
    TileBaseCacheClose(
      TileBaseCacheWriteMany(
        out=TileBaseCacheOpenWithRoot(opts.output,"w",root=TilesCommonRoot(ts,nout)),
        ts,nout));
    if(ts)   free(ts);
    if(root) free(root);
    if(TileBaseCacheError(out))
    { LOG("Error writing selected tiles to tilebase cache.\n\t%s\n\n",TileBaseCacheError(out));
      goto Error;
    }
  } else
  { tile_t *ts=0;
    int i,n=0;  
    TRY(ts=TileBaseArray(tb));
    for(i=0;i<TileBaseCount(tb);++i)
    { if(AABBHit(qbox,TileAABB(ts[i])))
      { printf("%s\n",TilePath(ts[i]));
        n++;
      }
    }
    fprintf(stderr,"Count: %d\n",n);
  }

Finalize:
  TileBaseClose(tb);
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}