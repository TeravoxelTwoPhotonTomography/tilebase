/**
 * query-name for tilebase
 * Usage:
 *   query-name <path> <query>
 */

#include "tilebase.h"
#include "src/opts.h"
#include "tre/tre.h"
#include <stdio.h>
#include <stdlib.h>

#define LOG(...)          printf(__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)

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

int main(int argc,char*argv[])
{ opts_t opts;
  int isok,ecode=0;
  tiles_t tb=0;
  regex_t reg={0};
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
  // Just print out one best match
  { tile_t *ts=0;
    int i;
    int min=99999;
    TRY(ts=TileBaseArray(tb));
    for(i=0;i<TileBaseCount(tb);++i)
    { if(0==tre_regaexec(&reg,TilePath(ts[i]),&match,params,0))
      { //printf("[% 5d] %s\n", match.cost,TilePath(ts[i]));
        if(match.cost<min)
        { clear();
          min=match.cost;
        }
        if(match.cost==min)
          push(i);
      }
    }
    printf("Best Matches (score %d):\n",min);
    while((i=pop())>=0)
      printf("%s\n",TilePath(ts[i]));
  }

Finalize:
  TileBaseClose(tb);
  return ecode;
Error:
  ecode=1;
  goto Finalize;
}