#include "tre/tre.h"
#include "search.h"

#define LOG(...)          fprintf(stderr,__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("%s(%d) - %s()\n\t%s\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,msg1,msg2)
#define TRY(e)            do{ if(!(e)) {REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)

/* Use a global stack to hold search results, in case of multiples.
   Using this makes the search unsafe for threading.
   It's not ciritical; If you're considering refactoring to make this thread
   safe, just change the data structure.  One alternative would be to use
   TilesFilter().
   Another option would be to just make the stack local to the function.
 */
static struct stack_t 
{ tile_t** data;
  int n,sz;
} stack={0,0,0};

static int push(tile_t* v)
{
  if(stack.n>=(stack.sz-1))
  { stack.sz=1.2*stack.n+50;
    TRY(stack.data=realloc(stack.data,stack.sz*sizeof(tile_t*)));
  }
  stack.data[stack.n++]=v;
  return 1;
Error:
  return 0;
}
static void clear() { stack.n=0; }
static tile_t* pop()   { return (stack.n>0)?(stack.data[--stack.n]):(NULL); }
static int count() { return stack.n; }

// --- INTERFACE ---
tile_t* FindByName(
  tile_t *ts,
  size_t ntiles,
  const char* query,
  void (*notunique)(const char* query,tile_t **ts, int n))
{
  tile_t *t=0;
  regex_t reg={0};
  regaparams_t params={0};
  regamatch_t  match={0};
  TRY(ts && ntiles>0);
  TRY(0==tre_regcomp(&reg,query,REG_NOSUB));
  params.cost_ins   =2;
  params.cost_del   =2;
  params.cost_subst =2;
  params.max_cost   =1;
  params.max_ins    =1;
  params.max_del    =1;
  params.max_subst  =1;
  params.max_err    =1;
  // Find matching names
  { int i;
    int min=99999;
    for(i=0;i<ntiles;++i)
    { if(0==tre_regaexec(&reg,TilePath(ts[i]),&match,params,0))
      { if(match.cost<min)
        { clear();
          min=match.cost;
        }
        if(match.cost==min)
          push(ts+i);
      }
    }
    if(count()>1 && notunique!=0)
      notunique(query,stack.data,stack.n);
  }
  return pop();
Error:
  return 0;
}