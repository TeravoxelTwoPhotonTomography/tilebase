#include "tilebase.h"
#include "stdio.h" // for logging

#define ENDL     "\n"
#define LOG(...) fprintf(stderr,__VA_ARGS__) 
#define TRY(e)   do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)

/** \returns -1 if v is negative, 1 if v is possitive, otherwise 0.
 *  Note that below, don't need to worry about "near 0" cases.
 */
static int sign(float v) { return (0.0f<v)-(v<0.0f); }

/** \param[in] tiles  Tile database will be modified so all tiles have the new field of view.
 *  \param[in] x_nm   Width of a tile in nanometers.   Will be ignored if negative.
 *  \param[in] y_nm   Height of a tile in nanometers.  Will be ignored if negative. 
 *  \returns 1 on success, 0 otherwise.
 */
int fix_fov(tiles_t tiles, int64_t x_nm, int64_t y_nm)
{ size_t i;
  tile_t *ts=0;
  if(x_nm>0 || y_nm>0)
  { TRY(ts=TileBaseArray(tiles));
    for(i=0;i<TileBaseCount(tiles);++i)
    { aabb_t box=0;
      int64_t *ori=0,*shape_nm=0;
      float *transform=0;
      nd_t shape_px;
      size_t td; //# dims for transform

      // Need to fix transform
      TRY(transform=TileTransform(ts[i]));
      TRY(td=ndndim(shape_px=TileShape(ts[i])));
      // Need to fix bounding box
      TRY(box=TileAABB(ts[i]));
      TRY(AABBGet(box,0,&ori,&shape_nm));
      // Because only x and y are changing and there are no off diagonal 
      // contributions in that part of the transform (xy rotations/shear),
      // it's sufficient to just put in the new x and y pixel size's into
      // the scaling parts of the matrix.  Need to preserve flips though.
      //
      // If this assumption changes, this will have to be reevaluated.
      // But this is a fix-up function.  It's not intended to be general.

      if(x_nm>0)
      { transform[0]=sign(transform[0])*(x_nm/(float)ndshape(shape_px)[0]);
        shape_nm[0]=x_nm;
      }
      if(y_nm>0) 
      { transform[td+2]=sign(transform[td+2])*(y_nm/(float)ndshape(shape_px)[1]);
        shape_nm[1]=y_nm;
      }
    }
  }
  return 1;
Error:
  return 0;
}

