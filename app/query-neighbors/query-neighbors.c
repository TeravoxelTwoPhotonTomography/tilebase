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

const char* signstr(aabb_t ref, aabb_t other)
{ static char buf[1024]={0};
  size_t i,n;
  int64_t *a,*b;
  AABBGet(ref  ,&n,&a,0);
  AABBGet(other,0 ,&b,0);
  memset(buf,0,sizeof(buf));
  snprintf(buf,sizeof(buf),"[%s",(a[0]>b[0])?"-":((a[0]<b[0])?"+":" "));
  for(i=1;i<n;++i)
    strncat(buf,(a[i]>b[i])?" -":((a[i]<b[i])?" +":"  "),sizeof(buf));
  strncat(buf,"]",sizeof(buf));
  return buf;
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
    fprintf(stderr,"Count: %d\n",(int)nout);
    if(ts)   free(ts);
    if(root) free(root);
    if(TileBaseCacheError(out))
    { LOG("Error writing selected tiles to tilebase cache.\n\t%s\n\n",TileBaseCacheError(out));
      goto Error;
    }
  } else
  { tile_t *ts=0;
    size_t i,n=0;  
    TRY(ts=TilesFilter(TileBaseArray(tb),TileBaseCount(tb),&n,hit,qbox));
    for(i=0;i<n;++i)
      printf("%s %s\n",signstr(qbox,TileAABB(ts[i])),TilePath(ts[i]));
    if(ts) free(ts);
    fprintf(stderr,"Count: %d\n",(int)n);
  }

Finalize:
  TileBaseClose(tb);
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}