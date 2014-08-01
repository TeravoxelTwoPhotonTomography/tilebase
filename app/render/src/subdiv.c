#include "subdiv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <app/render/config.h>

#ifdef DEBUG
int breakme() {}
#else
#define breakme(...)
#endif

#define ENDL          "\n"
#define LOG(...)      fprintf(stderr,__VA_ARGS__)
#define TRY(e)        do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme(); goto Error;}} while(0)
#define NEW(T,e,N)    TRY((e)=(T*)malloc(sizeof(T)*(N)))
#define ZERO(T,e,N)   memset((e),0,(N)*sizeof(T))

/*
   Tile split for limited memory on the gpu.
   Division is linearly spaced on z.
*/

subdiv_t  make_subdiv(nd_t in, float *transform, int ndim,size_t free,size_t total)
{ subdiv_t ctx=0;
#if HAVE_CUDA
  TRY(ndndim(in)>=3); // assume there's a z.
  NEW(struct _subdiv_t,ctx,1);
  ZERO(struct _subdiv_t,ctx,1);
#define CEIL(num,den) (((num)+(den)-1)/(den))
  ctx->n=(int)(CEIL(ndnbytes(in)*2,free)); /* need 2 copies of the subarray. This is a ceil of req.bytes/free.bytes */
#undef CEIL
  TRY(ctx->n<ndshape(in)[2]);

  ctx->transform=(float*)malloc((ndim+1)*(ndim+1)*sizeof(float));
  memcpy(ctx->transform,transform,(ndim+1)*(ndim+1)*sizeof(float));
  ctx->d=ndim;
  ctx->zmax =ndshape(in)[2];
  if(ctx->n==1) // nothing to do - single iteration
  { ctx->carray=in;
    return ctx;
  }
  TRY(ctx->carray=ndreshape(
                ndcast(
                  ndref(ndinit(),nddata(in),ndkind(in)),
                  ndtype(in)),
                ndndim(in),ndshape(in)));
  memcpy(ndstrides(ctx->carray),ndstrides(in),(ndndim(in)+1)*sizeof(size_t));
  ctx->dz_px=ndshape(ctx->carray)[2]/ctx->n;        // max size of each block.
  ctx->n+=(ctx->dz_px*ctx->n<ndshape(ctx->carray)[2]); // need one last block if not evenly divisible
  ndshape(ctx->carray)[2]=ctx->dz_px; // don't adjust strides, will get copied to a correctly formatted array on the gpu
  LOG("\t%5s: %10d\n" "\t%5s: %10d\n","n",(int)ctx->n,"dz_px",(int)ctx->dz_px);
  {
    int r;
    ctx->dz_nm=(int64_t*)calloc(ndim+1,sizeof(int64_t));
    for(r=0;r<(ndim+1);++r)
      ctx->dz_nm[r]=ctx->dz_px*transform[(ndim+1)*r+2];
  }
  return ctx;
#endif
Error:
  return 0;
}

void free_subdiv(subdiv_t ctx)
{ if(ctx)
  { if(ctx->n>1)
      ndfree(ndref(ctx->carray,0,nd_unknown_kind));
    free(ctx->transform);
    free(ctx->dz_nm);
    free(ctx);
  }
};

nd_t subdiv_vol(subdiv_t ctx)       {return ctx->carray;}

float* subdiv_xform(subdiv_t ctx)   {return ctx->transform;}

int  next_subdivision(subdiv_t ctx)
{ if(++ctx->i<ctx->n)
  { TRY(ctx->n>1); // sanity check..remove once confirmed
    if((ctx->i+1)*ctx->dz_px>ctx->zmax) {// adjust last block if necessary
      int z=ctx->zmax-ctx->i*ctx->dz_px;
      if(z<=0) return 0; // wtf
      ndshape(ctx->carray)[2]=z; // don't adjust strides, will get copied to a correctly formatted array on the gpu
    }
    ndoffset(ctx->carray,2,ctx->dz_px);
    { int i;
      const int n=ctx->d+1;
      for(i=0;i<n;++i)
        ctx->transform[n*i+(n-1)]+=ctx->dz_nm[i];
    }
    return 1;
  }
Error:
  return 0;
}