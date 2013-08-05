/**
 * \file
 * Tilebase cache interface implementation.
 */
#define _CRT_SECURE_NO_WARNINGS

#include "nd.h"
#include "cache.h"
#include "metadata/metadata.h"
#include "core.priv.h"
#define YAML_DECLARE_STATIC // on windows this should be defined if we're using static linking of libyaml (which we are)
#include "yaml.h"
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h> 

#include <limits.h> // for PATH_MAX (for realpath)
#include <stdlib.h> // for realpath()

#ifdef _MSC_VER
 #define vscprintf(fmt,args) _vscprintf(fmt,args)
 #define PATHSEP "\\"
 #define va_copy(a,b) ((a)=(b))
 #define snprintf _snprintf
#else
 #define vscprintf(fmt,args) vsnprintf(NULL,0,fmt,args)
 #define PATHSEP "/"
#endif

#define ENDL "\n"
#define LOG(...) tblog(self,__VA_ARGS__)
#define REPORT(e,msg) LOG("%s(%d): %s()\n\t%s\n\t%s\n\n",__FILE__,__LINE__,__FUNCTION__,#e,msg)
#define TRY(e) do{if(!(e)) {REPORT(e,"Evaluated to false."); goto Error;}}while(0)
#define FAIL(msg) do{ REPORT(FAIL,msg); goto Error;} while(0)

#define NEW(T,e,N)    TRY((e)=(T*)malloc((N)*sizeof(T)))
#define ZERO(T,e,N)   TRY(memset((e),0,(N)*sizeof(T)))
#define RESIZE(T,e,N) TRY((e)=(T*)realloc((e),(N)*sizeof(T)))

//
// === TILEBASE CACHE CONTEXT ===
//
#define READ  (0)
#define WRITE (1)
static void tblog(tilebase_cache_t a,const char *fmt,...); ///< Logging function.  forward declared.

struct _tilebase_cache_t
{ int mode;
  union _ctx
  { struct _reader
    { struct _seq 
      { union _b {
          int64_t *i64;
          double  *f64;
        } b;
        size_t sz,cap;
      } seq;                  ///< resizeable, temporary storage for int64_t sequences.
      void *state;            ///< a 1 element stack for tracking parser state
      void (*callback)(tilebase_cache_t); ///< notifier called after the state stack gets popped
      yaml_parser_t parser;
      tiles_t       tiles;  ///< the tiles collection under construction.
    } reader;

    struct _writer
    { yaml_emitter_t emitter;
    } writer;

  } ctx;
  FILE *fp;
  yaml_event_t  event;
  char  rootpath[1024];
  char *log;
};

// ACCESSORS
#define ROOT     ( self->rootpath)
#define PARSER   (&self->ctx.reader.parser)
#define EMITTER  (&self->ctx.writer.emitter)
#define EVENT    (&self->event)
#define E_TYPE   ( self->event.type)
#define E_VAL    ((char*) self->event.data.scalar.value) // !!! unsigned char* to char* conversion
#define STATE    ( self->ctx.reader.state)
#define FP       (self->fp)
#define TILES    (self->ctx.reader.tiles)
#define LASTTILE (TILES->tiles[TILES->sz-1])
#define SEQ_N    (self->ctx.reader.seq.sz)
#define SEQ_CAP  (self->ctx.reader.seq.cap)
#define SEQI     (self->ctx.reader.seq.b.i64)
#define SEQF     (self->ctx.reader.seq.b.f64)

#define KEY(key) (strcmp((char*)EVENT->data.scalar.value,key)==0)

#define EMIT           do {TRY(yaml_emitter_emit(EMITTER,EVENT));/*yaml_event_delete(EVENT);*/}while(0)
#define MAP_START      TRY(yaml_mapping_start_event_initialize(EVENT,0,0,1,YAML_ANY_MAPPING_STYLE))
#define MAP_END        TRY(yaml_mapping_end_event_initialize(EVENT))
#define SEQ_START      TRY(yaml_sequence_start_event_initialize(EVENT,0,0,1,YAML_ANY_SEQUENCE_STYLE))
#define SEQ_START_FLOW TRY(yaml_sequence_start_event_initialize(EVENT,0,0,1,YAML_FLOW_SEQUENCE_STYLE))
#define SEQ_END        TRY(yaml_sequence_end_event_initialize(EVENT))
#define SCALAR(str)    TRY(yaml_scalar_event_initialize(EVENT,0,0,(yaml_char_t*)str,(int)strlen(str),1,1,YAML_ANY_SCALAR_STYLE))

// HELPERS


#ifdef _MSC_VER
#include <windows.h>
#undef  LOG
#define LOG(...) fprintf(stderr,__VA_ARGS__)
#define PATH_MAX (MAX_PATH)
/** Stand-in for realpath() for windows that uses GetFullPathName().
 *  Only conforms to how realpath is used here.  Does not conform to POSIX.
 */
static char* realpath(const char* path,char *out)
{ int n=0;
  TRY(n=GetFullPathName(path,out?MAX_PATH:0,out,NULL));
  if(!out)
  { NEW(char,out,n);
    ZERO(char,out,n);
    TRY(GetFullPathName(path,n,out,NULL));
  }
  return out;
Error:
  return 0;
}

#undef LOG
#define LOG(...) tblog(self,__VA_ARGS__) // restore the normal logger
#endif // _MSC_VER

#undef  LOG
#define LOG(...) fprintf(stderr,__VA_ARGS__)
/** If realpath fails, just pass path to out.
    Always allocates out if out is passed in as NULL.
 */
static char* maybeRealPath(const char* path, char *out)
{ char *o=0;
  if(!(o=realpath(path,out)))
  { o=out;
    if(!o)
      NEW(char,o,strlen(path)+1);
    ZERO(char,o,strlen(path)+1);
    memcpy(o,path,strlen(path));
  }
  return o;
Error:
  return 0;
}
#undef  LOG
#define LOG(...) tblog(self,__VA_ARGS__) // restore the normal logger

/** Returns the number of elements in an affine transform for an
 *  \a n-dimensional vector.
 */
static size_t ntransform(size_t n) { return (n+1)*(n+1); }

/** Find first difference between full-path and the root-path stored in the
    tilebase cache context.
*/
static const char* relative(tilebase_cache_t self, const char* fullpath)
{ const char* c;
  for(c=ROOT;*c&&*fullpath;c++,fullpath++)
    if(*c!=*fullpath)
      break;
  return (*fullpath)?fullpath:"";
}

/** Join the root path stored in the tilebase cache context with the relative
    path in \a rpath.  The result is stored in \a dst, which she preallocated
    and long enough to store the result.

    It is assumed that rpath already has the path sepeartor, so this is pretty
    much just a strcat.
*/
static void join(char *dst, tilebase_cache_t self, char *rpath)
{ 
#if _MSC_VER
  { size_t i,n=strlen(rpath);
    for(i=0;i<n;++i) if(rpath[i]=='/') rpath[i]='\\';
  }
#endif
  memcpy(dst,ROOT,strlen(ROOT));
  strcat(dst,rpath);
}

static unsigned push_back_tile(tilebase_cache_t self)
{ tile_t t=0;
  NEW(struct _tile_t,t,1);
  ZERO(struct _tile_t,t,1);
  if(++TILES->sz>=TILES->cap)
  { TILES->cap=(size_t)(TILES->cap*1.2+50);
    RESIZE(tile_t,TILES->tiles,TILES->cap);
  }
  LASTTILE=t;
  return 1;
Error:
  if(t) free(t);
  return 0;
}

static
const char* nd_type_to_str(nd_type_id_t id)
{ static const char *names[]={
    "u8",  "u16",  "u32",  "u64",
    "i8",  "i16",  "i32",  "i64",
                   "f32",  "f64"};
  return names[id];
}
static nd_type_id_t str_to_nd_type(const char* str)
{ int i;
  for(i=0;i<nd_id_count;++i)
    if(strcmp(nd_type_to_str((nd_type_id_t)i),str)==0)
      return (nd_type_id_t)i;
  return nd_id_unknown;
}

//
// === ERROR LOGGING ===
//

/** Appends message to error log for \a a/ */
static 
void tblog(tilebase_cache_t a,const char *fmt,...)
{ size_t n,o;
  va_list args,args2;
  if(!a) goto Error;
  va_start(args,fmt);
  va_copy(args2,args);      // [HAX] this is pretty ugly
  n=vscprintf(fmt,args)+1; // add one for the null terminator
  va_end(args);
  if(!a->log) a->log=(char*)calloc(n,1);
  //append
  o=strlen(a->log);
  a->log=(char*)realloc(a->log,n+o);
  memset(a->log+o,0,n);
  if(!a->log) goto Error;
  va_copy(args,args2);
  vsnprintf(a->log+o,n,fmt,args2);
  va_end(args2);
  va_end(args);
  return;
Error:
  va_start(args2,fmt);
  vfprintf(stderr,fmt,args2); // print to stderr if logging fails.
  va_end(args2);
  if(a) // if logging object is null, the error has already been reported
    fprintf(stderr,"%s(%d): Logging failed."ENDL,__FILE__,__LINE__);
}

//
// === PARSER ===
//
typedef void* (*handler_t)(tilebase_cache_t self);
// handlers - forward declared
static void* doc(tilebase_cache_t self);
static void* root(tilebase_cache_t self);
static void* tile(tilebase_cache_t self);
static void* shape(tilebase_cache_t self);
static void* aabb(tilebase_cache_t self);
static void* sequence_of_ints(tilebase_cache_t self);
static void* sequence_of_floats(tilebase_cache_t self);
// sequence handlers - forward declared
static void  ori(tilebase_cache_t self);
static void  box(tilebase_cache_t self);
static void  dims(tilebase_cache_t self);
static void  transform(tilebase_cache_t self);
// state stack manipulation
static void* pop(tilebase_cache_t self);
static void  push(tilebase_cache_t self, void *f, void *callback);
// resizable integer sequence buffer
static int   append_i64(tilebase_cache_t self, const char* str);
static int   append_f64(tilebase_cache_t self, const char* str);
static void  clear(tilebase_cache_t self);
static void  release(tilebase_cache_t self);

void push(tilebase_cache_t self, void *f, void *callback)
{ self->ctx.reader.state=f;
  self->ctx.reader.callback=callback;
}
void* pop(tilebase_cache_t self)
{ void *f=self->ctx.reader.state;
  self->ctx.reader.state=0;
  if(self->ctx.reader.callback)
    self->ctx.reader.callback(self);  
  return (self->log)?0:f;
}
int append_i64 (tilebase_cache_t self, const char* str)
{ if(SEQ_N>=SEQ_CAP)
  { SEQ_CAP=(size_t)(SEQ_CAP*1.2+50);
    RESIZE(int64_t,SEQI,SEQ_CAP);
  }
  errno=0;
  SEQI[SEQ_N++]=strtol(str,NULL,10);
  TRY(!errno);
  return 1;
Error:
  REPORT(errno,strerror(errno));
  return 0;
}
int append_f64 (tilebase_cache_t self, const char* str)
{ if(SEQ_N>=SEQ_CAP)
  { SEQ_CAP=(size_t)(SEQ_CAP*1.2+50);
    RESIZE(double,SEQF,SEQ_CAP);
  }
  errno=0;
  SEQF[SEQ_N++]=strtod(str,NULL);
  TRY(!errno);
  return 1;
Error:
  REPORT(errno,strerror(errno));
  return 0;
}
void clear(tilebase_cache_t self)       {SEQ_N=0;}
void release(tilebase_cache_t self)     {if(SEQI) free(SEQI);}
void *sequence_of_ints(tilebase_cache_t self)
{ switch(E_TYPE)
  { case YAML_SEQUENCE_START_EVENT: clear(self); return sequence_of_ints;
    case YAML_SEQUENCE_END_EVENT: return pop(self);
    case YAML_SCALAR_EVENT: TRY(append_i64(self,E_VAL)); return sequence_of_ints;
    default:;
  }
Error:
  return 0;
}
void *sequence_of_floats(tilebase_cache_t self)
{ switch(E_TYPE)
  { case YAML_SEQUENCE_START_EVENT: clear(self); return sequence_of_floats;
    case YAML_SEQUENCE_END_EVENT: return pop(self);
    case YAML_SCALAR_EVENT: TRY(append_f64(self,E_VAL)); return sequence_of_floats;
    default:;
  }
Error:
  return 0;
}
void ori(tilebase_cache_t self)
{ TRY(AABBSet(LASTTILE->aabb,SEQ_N,SEQI,0));
  Error:;// pass
}
void aabb_shape(tilebase_cache_t self)
{ TRY(AABBSet(LASTTILE->aabb,SEQ_N,0,SEQI));
  Error:;// pass
}
void dims(tilebase_cache_t self)
{ size_t i;
  for(i=0;i<SEQ_N;++i)
    TRY(ndShapeSet(LASTTILE->shape,(unsigned int)i,(size_t)SEQI[i]));
  Error:;// pass
}
void transform(tilebase_cache_t self)
{ size_t i;
  RESIZE(float,LASTTILE->transform,SEQ_N);
  for(i=0;i<SEQ_N;++i)
    LASTTILE->transform[i]=(float)SEQF[i];
  Error:; //pass
}

void* aabb(tilebase_cache_t self)
{ switch(E_TYPE)
  { case YAML_MAPPING_START_EVENT:
      LASTTILE->aabb=AABBMake(0);
      return aabb;
    case YAML_MAPPING_END_EVENT: return tile;
    case YAML_SCALAR_EVENT: 
      if(KEY("ori"))
      { push(self,aabb,ori);
        return sequence_of_ints;
      } else if(KEY("shape"))
      { push(self,aabb,aabb_shape);
        return sequence_of_ints;
      }
    default:;
  }
  return 0;
}

void* shape(tilebase_cache_t self)
{ switch(E_TYPE)
  { case YAML_MAPPING_START_EVENT: 
      TRY(LASTTILE->shape=ndinit());
      return shape;
    case YAML_MAPPING_END_EVENT: return tile;
    case YAML_SCALAR_EVENT: 
      if(KEY("type"))
      { yaml_event_delete(EVENT);
        TRY(yaml_parser_parse(PARSER,EVENT));
        TRY(ndcast(LASTTILE->shape,str_to_nd_type(E_VAL)));
        return shape;
      } else if(KEY("dims"))
      { push(self,shape,dims);
        return sequence_of_ints;
      }
    default: return shape;
  }
Error:
  return 0;
}

void* tile(tilebase_cache_t self)
{ switch(E_TYPE)
  { case YAML_MAPPING_START_EVENT:
      push_back_tile(self);
      return tile;
    case YAML_MAPPING_END_EVENT: return tile;
    case YAML_SCALAR_EVENT:
      if(KEY("path"))
      { yaml_event_delete(EVENT);
        TRY(yaml_parser_parse(PARSER,EVENT));
        join(LASTTILE->path,self,E_VAL);
        return tile;
      }
      else if(KEY("aabb")) { return aabb;}
      else if(KEY("shape")){ return shape;}
      else if(KEY("transform"))
      { push(self,tile,transform);
        return sequence_of_floats;
      }
      printf("Unrecognized: %s\n",E_VAL);
    default:;
  }
Error:
  return 0;
}

void* root(tilebase_cache_t self)
{ switch(E_TYPE)
  { case YAML_MAPPING_START_EVENT: return root;
    case YAML_SCALAR_EVENT: // consume key, consume+parse value, consume key
      yaml_event_delete(EVENT);
      TRY(yaml_parser_parse(PARSER,EVENT)); // consume key without checking it.. should be "path"
      { char *rpath=0;
        TRY(rpath=maybeRealPath(E_VAL,NULL));
      //printf("\t%s: %s\n","Root path",rpath);
        memcpy(ROOT,rpath,strlen(rpath));
        free(rpath);
      }
      yaml_event_delete(EVENT);
      TRY(yaml_parser_parse(PARSER,EVENT)); // consume key without checking it.. should be "tiles"
      return root;
    case YAML_SEQUENCE_START_EVENT: return tile;
    default:;
  }
Error:
  return 0;
}

/** Only reads first document in the stream*/
void* doc(tilebase_cache_t self)
{ if(E_TYPE!=YAML_DOCUMENT_START_EVENT)
    return 0;
  NEW(struct _tiles_t,TILES,1);
  ZERO(struct _tiles_t,TILES,1);
  return root;
Error:
  return 0;
}

//
// === EMITTER ===
//

unsigned emit_seq_i64(tilebase_cache_t self, size_t n, int64_t *s)
{ size_t i;
  char buf[1024];
  SEQ_START_FLOW; EMIT;
  for(i=0;i<n;++i)
  { snprintf(buf,sizeof(buf),"%lld",s[i]);
    SCALAR(buf); EMIT;
  }
  SEQ_END; EMIT;
  return 1;
Error:
  return 0;
}

unsigned emit_seq_sz(tilebase_cache_t self, size_t n, size_t *s)
{ size_t i;
  char buf[1024];
  SEQ_START_FLOW; EMIT;
  for(i=0;i<n;++i)
  { snprintf(buf,sizeof(buf),"%llu",(unsigned long long)s[i]);
    SCALAR(buf); EMIT;
  }
  SEQ_END; EMIT;
  return 1;
Error:
  return 0;
}

unsigned emit_seq_f32(tilebase_cache_t self, size_t n, float *s)
{ size_t i;
  char buf[1024];
  SEQ_START_FLOW; EMIT;
  for(i=0;i<n;++i)
  { snprintf(buf,sizeof(buf),"%f",(float)s[i]);
    SCALAR(buf); EMIT;
  }
  SEQ_END; EMIT;
  return 1;
Error:
  return 0;
}
//
// === INTERFACE ===
//

/**
 * \param[in] path  Path to the root tile directory.
 * \param[in] mode  A string.  Either "r" or "w"
 */
tilebase_cache_t TileBaseCacheOpen (const char *path, const char *mode)
{ tilebase_cache_t self=0;
  char fpath[1024]={0},*rpath=0;
  const char fname[]="tilebase.cache.yml";

  strncpy(fpath,path,sizeof(fpath)-1);
  strncat(fpath,PATHSEP,sizeof(fpath)-strlen(fpath)-1);
  strncat(fpath,fname,sizeof(fpath)-strlen(fpath)-1);

  TRY(rpath=maybeRealPath(path,NULL));
  self=TileBaseCacheOpenWithRoot(fpath,mode,rpath);
  if(rpath) free(rpath);
  return self;
Error:
  if(rpath) free(rpath);
  LOG("\tpath: %s"ENDL "\tmode: %s"ENDL,fpath[0]?fpath:"(none)",mode?mode:"(none)");
  TileBaseCacheClose(self);
  return 0;
}

/**
 * Read/writes a tilebase cache file to filename.
 * 
 * When writing \a root specifies the root path element against which to
 * relatively find tiles.
 *
 * When reading, if not NULL, \a root should point to an allocated array of
 * MAX_PATH chars.  This will be filled with the root path element specifed in
 * the opened cache file.
 */
tilebase_cache_t TileBaseCacheOpenWithRoot(const char *fpath, const char *mode, char* rpath)
{ tilebase_cache_t self;

  NEW(struct _tilebase_cache_t,self,1);
  ZERO(struct _tilebase_cache_t,self,1);
  TRY(FP=fopen(fpath,mode));
  switch(mode[0])
  { case 'r':
      self->mode=READ;
      TRY(yaml_parser_initialize(PARSER));
      yaml_parser_set_input_file(PARSER,FP);
      if(rpath)
      { memcpy(rpath,ROOT,strlen(ROOT));
        rpath[strlen(ROOT)]='\0';
      }
      break;
    case 'w':
      self->mode=WRITE;
      TRY(yaml_emitter_initialize(EMITTER));
      yaml_emitter_set_output_file(EMITTER,FP);
      yaml_stream_start_event_initialize(EVENT, YAML_UTF8_ENCODING);
      EMIT;
      TRY(strlen(rpath)<sizeof(ROOT));
      memcpy(ROOT,rpath,strlen(rpath));
      TRY(yaml_document_start_event_initialize(EVENT,NULL,NULL,NULL,0));
      EMIT;
      MAP_START;       EMIT;
      SCALAR("path");  EMIT;
      SCALAR(rpath);   EMIT;
      SCALAR("tiles"); EMIT;
      SEQ_START;       EMIT;
      break;
    default: FAIL("Unrecognized mode.");
  }
  return self;
Error:
  LOG("\tpath: %s"ENDL "\tmode: %s"ENDL,fpath[0]?fpath:"(none)",mode?mode:"(none)");
  TileBaseCacheClose(self);
  return 0;
}

void TileBaseCacheClose(tilebase_cache_t self)
{ if(!self) return;
  if(self->log)
  {  printf("[TileBaseCache]\n\t%s\n",self->log);
     free(self->log);
  }
  //else if(self->mode==WRITE)// write the footer if everything is ok

  switch(self->mode)
  { case READ:
      yaml_parser_delete(PARSER);
      release(self);
      if(TILES) TileBaseClose(TILES);
      break;
    case WRITE:
      { SEQ_END; EMIT;
        MAP_END; EMIT;
        yaml_document_end_event_initialize(EVENT,0);
        EMIT;
      }
      yaml_emitter_delete(EMITTER);
      break;
    default:;
  }
  yaml_event_delete(EVENT);
Error: // may memory leak if there's an error in this function.
  if(FP) fclose(FP);
  free(self);
}

/**
 * Reads a tile data base from a cache file.
 * Example:
 * \code{c}
 * tiles_t tiles;
 * TileBaseCacheClose(TileBaseCacheRead(TileBaseCacheOpen(path,"r"),&tiles));
 * \endcode
 * 
 * \param[in]  self  A \a tilebase_cache_t opened in read mode. \see TileBaseCacheOpen().
 * \param[out] tiles A pointer to a \a tiles_t tile collection.  The object pointed
 *                   to by <tt>(*tiles)</tt> should not be initialized.  If there 
 *                   is an error and \a tiles is not NULL it will be set to zero.
 *                   On success, \a tiles is set to the newly contructed tile
 *                   collection.
 * \returns 0 on error, otherwise \a self.
 */
tilebase_cache_t TileBaseCacheRead (tilebase_cache_t self, tiles_t *tiles)
{ handler_t state;
  TRY(self);
  TRY(tiles); //may not be NULL;
  TRY(yaml_parser_parse(PARSER,EVENT));
  for(state=doc;state;state=(handler_t)state(self))
  { yaml_event_delete(EVENT);
    TRY(yaml_parser_parse(PARSER,EVENT));
  }
  *tiles=TILES;
  TILES=0;
  return self;
Error:
  if(tiles) *tiles=0;
  return 0;
} 


tilebase_cache_t TileBaseCacheWrite(tilebase_cache_t self, const char* path_, tile_t t)
{ size_t ndim;
  int64_t *ori,*shape;
  nd_type_id_t tid;
  size_t *dims;
  char path[PATH_MAX+1]={0};
  TRY(self);
  TRY(maybeRealPath(path_,path));// canonicalize input path
  // assert all the attributes we need to read exist.
  TRY(AABBGet(TileAABB(t),&ndim,&ori,&shape));
  tid=ndtype(TileShape(t));
  TRY(dims=ndshape(TileShape(t)));
  // === EMIT TILE ===
  MAP_START; EMIT;
    SCALAR("path"); EMIT;
    SCALAR(relative(self,path)); EMIT;
    SCALAR("aabb"); EMIT;
    MAP_START; EMIT;
      SCALAR("ori"); EMIT;
      TRY(emit_seq_i64(self,ndim,ori));
      SCALAR("shape"); EMIT;
      TRY(emit_seq_i64(self,ndim,shape));
    MAP_END; EMIT;
    SCALAR("shape"); EMIT;
    MAP_START; EMIT;
      SCALAR("type"); EMIT;
      SCALAR(nd_type_to_str(tid)); EMIT;
      SCALAR("dims"); EMIT;
      emit_seq_sz(self,ndndim(TileShape(t)),dims);
    MAP_END; EMIT;
    SCALAR("transform"); EMIT;
    emit_seq_f32(self,ntransform(ndndim(TileShape(t))),TileTransform(t));
  MAP_END; EMIT;

  return self;
Error:
  return 0;
}

tilebase_cache_t TileBaseCacheWriteMany(tilebase_cache_t self, tile_t *t, size_t ntiles)
{ size_t i;
  TRY(self);
  for(i=0;i<ntiles;++i)
    self=TileBaseCacheWrite(self,TilePath(t[i]),t[i]);
  return self;
Error:
  return 0;
}

char* TileBaseCacheError(tilebase_cache_t self)
{ return (self)?self->log:0;

}
