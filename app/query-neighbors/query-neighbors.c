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

// caller must free returned value
// Usage: TRY(out=filter(in,n,&nout,testfun,ctx));
tile_t *filter(tile_t *in, size_t n, size_t *nout, unsigned (*test)(tile_t *a,void *ctx), void *ctx)
{ size_t i,c=0;
  tile_t *out=0;
  NEW(tile_t,out,n);
  ZERO(tile_t,out,n);
  for(i=0;i<n;++i)
    if(test(in+i,ctx))
      out[c++]=in[i];
  if(nout) *nout=c;
  return out;
Error:
  if(out) free(out);
  return 0;
}

unsigned hit(tile_t *a, void *ctx)
{ aabb_t box=(aabb_t)ctx;
  return AABBHit(TileAABB(*a),box);
}

unsigned ispathsep(char c)
{ switch(c)
  {
#ifdef _MSC_VER
    case '\\':
#endif
    case '/': return 1;
  default:
    return 0;
  }
}

// caller must free returned string
char* commonroot(const tile_t* tiles, size_t ntiles)
{ size_t n,i,j,s; // s marks the last path sep
  const char *ref=0;
  TRY(ntiles>0);
  n=strlen(ref=TilePath(tiles[0]))+1; // n is first diff, so one past last
  for(i=1;i<ntiles;++i)
  { const char *b=TilePath(tiles[i]);
    n=min(n,strlen(b)+1);
    for(j=0;j<n && (b[j]==ref[j]);j++)
      if(ispathsep(b[j]))
        s=j;
    n=min(j,s+1);
  }
  { char *out=0;
    NEW(char,out,n+1);
    memcpy(out,ref,n);
    out[n]='\0';
    return out;
  }
Error:
  return 0;
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
    TRY(ts=filter(TileBaseArray(tb),TileBaseCount(tb),&nout,hit,qbox));
    TileBaseCacheClose(
      TileBaseCacheWriteMany(
        out=TileBaseCacheOpenWithRoot(opts.output,"w",root=commonroot(ts,nout)),
        ts,nout));
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