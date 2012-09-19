/** \file
    Interface for reading and writing tile metadata.

    At the moment the tile metadata describes:

    1. The number of color channels.
    2. Properties that deterimine the bounding box of the tile.

*/
#include <stdlib.h>
#include "tilebase.h"
//#include "nd.h"
//#include "../aabb.h"
//#include "../core.h"
#include "metadata.h"
#include "interface.h"
#include "plugin.h"
#include "config.h"
#include <stdio.h>

/// @cond DEFINES
#define ENDL              "\n"
#define LOG(...)          fprintf(stderr,__VA_ARGS__)
#define TRY(e)            do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define NEW(type,e,nelem) TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define SAFEFREE(e)       if(e){free(e); (e)=NULL;}    
#define countof(e)        (sizeof(e)/sizeof(*(e)))
/// @endcond


/** Context for metadata io. */
struct _metadata_t
{ metadata_api_t *fmt; ///< The format specific implementation
  void           *ctx; ///< format specific file context
	char           *log; ///< (UNUSED) Error log.  NULL if no error. 
};

//
// FORMAT REGISTRY
//

static metadata_api_t** g_formats=NULL;      ///< metadata format registry
static size_t           g_countof_formats=0; ///< number of loaded metadata formats

/**
 * Find metadata formats and initialize \a g_formats. 
 * \todo make thread safe
 */
static int maybe_load_plugins()
{ size_t i;
	if(g_formats) return 1;
  TRY(g_formats=MetadataLoadPlugins(METADATA_PLUGIN_PATH,&g_countof_formats));
	return 1;
Error:
	return 0;
}

/** \returns the index of the detected format on sucess, otherwise -1 */
static int detect_file_type(const char *filename, const char *mode)
{ size_t i;
  TRY(filename);
  TRY(mode);
  for(i=0;i<g_countof_formats;++i)
    if(g_formats[i]->is_fmt(filename,mode))
      return (int)i;
Error:
  return -1;
}

/** \returns the index of the detected format on sucess, otherwise -1 */
static int get_format_by_name(const char *format)
{ size_t i;
  if(!format) return -1;
  for(i=0;i<g_countof_formats;++i)
    if(0==strcmp(format,g_formats[i]->name()))
      return (int)i;
  return -1;
}

//
// INTERFACE
//

unsigned MetadataFormatCount()
{ maybe_load_plugins();
  return g_countof_formats;
}
const char* MetadataFormatName(unsigned i)
{ if(i>=MetadataFormatCount()) return NULL;
  return g_formats[i]->name();
}

/** Get abstract context. \returns the format specific context on success, otherwise 0.*/
void* MetadataContext(metadata_t self) {return self?self->ctx:0;}
/** Get error string. \returns an error string if there was an error, otherwise 0.*/
char* MetadataError(metadata_t self)   {return self?self->log:0;}

/** Detect the presence of readible metadata as \a path. */
unsigned MetadataIsFound(const char *tilepath)
{ maybe_load_plugins();
	return detect_file_type(tilepath,"r")>0;
}

/**
 * Open metadata according to the mode.
 */
metadata_t MetadataOpen(const char *path, const char *format, const char *mode)
{ metadata_t file=NULL;
  void *ctx=NULL;
  int ifmt;
  maybe_load_plugins();
  if(format && *format)
  { if(0>(ifmt=get_format_by_name(format))) goto ErrorSpecificFormat;
  } else
  { if(0>(ifmt=detect_file_type(path,mode))) goto ErrorDetectFormat;
  }
  TRY(ctx=g_formats[ifmt]->open(path,mode));
  NEW(struct _metadata_t,file,1);
  file->ctx=ctx;
  file->fmt=g_formats[ifmt];
  file->log=NULL;
  return file;
ErrorSpecificFormat:
  LOG("%s(%d): %s"ENDL "\tCould not open \"%s\" for %s with specified format %s."ENDL,
      __FILE__,__LINE__,__FUNCTION__,path?path:"(null)",(mode[0]=='r')?"reading":"writing",format);
  return NULL;
ErrorDetectFormat:
  LOG("%s(%d): %s"ENDL "\tCould not open \"%s\" for %s."ENDL,
      __FILE__,__LINE__,__FUNCTION__,path?path:"(null)",(mode[0]=='r')?"reading":"writing"); 
  return NULL;
Error:
  return NULL; 
}

/** Closes the file and releases resources.  Always succeeds. */
void MetadataClose(metadata_t self)
{ if(!self) return;
  self->fmt->close(self);
  if(self->log) fprintf(stderr,"Log: 0x%p"ENDL "\t%s"ENDL,self,self->log);
  SAFEFREE(self->log);
  free(self);
}

unsigned MetadataGetShape(metadata_t self, size_t *nelem, int64_t *shape)
{ TRY(self&&self->fmt->shape);
  TRY(nelem);
	return self->fmt->shape(self,nelem,shape);
Error:
	return 0;
}
unsigned MetadataSetShape(metadata_t self, size_t  nelem, int64_t *shape)
{ TRY(self&&self->fmt->set_shape);
  TRY(shape);
	return self->fmt->set_shape(self,nelem,shape);
Error:
	return 0;
}
unsigned MetadataGetOrigin(metadata_t self, size_t *nelem, int64_t *ori)
{ TRY(self&&self->fmt->origin);
  TRY(nelem);
	return self->fmt->origin(self,nelem,ori);
Error:
	return 0;
}
unsigned MetadataSetOrigin(metadata_t self, size_t  nelem, int64_t *ori)
{ TRY(self&&self->fmt->set_origin);
  TRY(ori);
	return self->fmt->set_origin(self,nelem,ori);
Error:
	return 0;
}
ndio_t MetadataOpenVolume(metadata_t self, const char* mode)
{ TRY(self&&self->fmt->get_vol);
  return self->fmt->get_vol(self,mode);
Error:
  return 0;
}

/**
 * Read tile bounding box from the metadata.
 * 
 * Assume's the tile->aabb has been init'd, but the file hasn't been opened.
 */
unsigned MetadataGetTileAABB(metadata_t self, tile_t tile)
{ size_t ndim;
  aabb_t bbox;
  TRY(MetadataGetOrigin(self,&ndim,NULL));      // Get ndim
  TRY(bbox=TileAABB(tile));                  // 
  AABBSet(bbox,ndim,0,0);                    // This will alloc space in the bbox
  { int64_t *ori=0,*shape=0;
    AABBGet(bbox,NULL,&ori,&shape);          // Get the alloc'd ptrs in bbox
    TRY(MetadataGetOrigin(self,&ndim,ori));  // Now copy the origin and shape
    TRY(MetadataGetShape(self,&ndim,shape));
  }
  return 1;
Error:
  return 0;
}

/**
 * Save bounding box data from the tile to the metadata.
 */
unsigned MetadataSetTileAABB(metadata_t self, tile_t tile)
{ int64_t *ori,*shape;
  size_t n;
  aabb_t bbox;
  TRY(bbox=TileAABB(tile));
  AABBGet(bbox,&n,&ori,&shape);
  TRY(MetadataSetShape(self,n,shape));
  TRY(MetadataSetOrigin(self,n,ori));
  return 1;
Error:
  return 0;
}

/**
 * Read/compute the (ndim+1)x(ndim+1) projective transform for the tile,
 * where \a ndim is ndndim(ndioShape(MetadataOpenVolume(self))).
 * 
 * \param[in,out]   transform   Must be preallocated.  Recieves the transform.
 * \returns 1 on success, 0 otherwise.
 */
unsigned MetadataGetTransform(metadata_t self, float *transform)
{ TRY(self&&self->fmt->get_transform);
  TRY(transform);
  return self->fmt->get_transform(self,transform);
Error:
  return 0;
}