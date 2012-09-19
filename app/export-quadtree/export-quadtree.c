/**
 * \file
 * Generate a quadtree
 */
#include <stdio.h>
#include "tilebase.h"
#include "opts.h"

#define PATHSEP  '/'
#define ENDL     "\n"
#define LOG(...) fprintf(stderr,__VA_ARGS__) 
#define TRY(e)   do{if(!(e)) { DBG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)

//#define DEBUG
#ifdef DEBUG
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif

int main(int argc, char* argv[])
{ unsigned ecode=0;
  tiles_t tiles=0;
  opts_t opts;
  TRY(parse_args(&opts,argc,argv));
  printf("OPTS: %s %s\n",opts.src,opts.dst);
  TRY(tiles=TileBaseOpen(opts.src,opts.src_format));
  //TRY(render(tiles,TileBaseAABB(tiles)));
Finalize:
  TileBaseClose(tiles);
  return ecode;
Error:
  ecode=1;
  goto Finalize;

}

