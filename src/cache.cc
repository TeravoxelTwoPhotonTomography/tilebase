/**
 * \file
 * Functions for reading and writing a cached tiles_t structure to/from disk.
 */
#include <fstream>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "yaml-cpp/yaml.h"
#include "aabb.h"
#include "nd.h"
#include "tilebase.h"
#include "cache.h"
#include "core.priv.h"

using namespace YAML;
using namespace std;

#ifdef _MSC_VER
#pragma warning(disable:4996) // disable security deprecation warning for vsnprintf and such
#define va_copy(a,b) ((a)=(b))
#define PATHSEP "\\"
#else
#define PATHSEP "/"
#endif

#define ENDL               "\n"
#define LOG(...)           log(self,__VA_ARGS__)
#define REPORT(e,msg)      LOG("%s(%d): %s()"ENDL "\t%s"ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e,msg)
#define TRY(e)             do{if(!(e)) { REPORT((e),"Expression evaluated as false."); goto Error;}} while(0)
#define FAIL(msg)          do{ REPORT(Failure,msg); goto Error; } while(0)

#define NEW(type,e,nelem)  TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define ZERO(type,e,nelem) memset((e),0,sizeof(type)*nelem)


#define MODE_UNK   (-1)
#define MODE_READ  (0)
#define MODE_WRITE (1)

struct _tilebase_cache_t
{ int mode;
  string rootpath;
  std::fstream in;
  std::ofstream out;
  Emitter emitter;
  char *log;
  _tilebase_cache_t(const char* path) : mode(MODE_UNK), rootpath(path), log(0) {}
  ~_tilebase_cache_t() {if(log) free(log);}

  const char* relative(const char* fullpath)
  { const char *c;
    for(c=rootpath.c_str();*c&&*fullpath;c++,fullpath++)
      if(*c!=*fullpath)
        break;
    return (*fullpath)?fullpath:"";
  }
};

/** Appends message to error log for \a a/
  */
static 
void log(tilebase_cache_t a,const char *fmt,...)
{ size_t n,o;
  va_list args,args2;
  if(!a) goto Error;
  va_start(args,fmt);
  va_copy(args2,args);      // [HAX] this is pretty ugly
#ifdef _MSC_VER
  n=_vscprintf(fmt,args)+1; // add one for the null terminator
#else
  n=vsnprintf(NULL,0,fmt,args)+1;
#endif
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
#if 0
  vfprintf(stderr,fmt,args);
#endif
  va_end(args);
  return;
Error:
  va_start(args2,fmt);
  vfprintf(stderr,fmt,args2); // print to stderr if logging fails.
  va_end(args2);
  if(a) // if logging object is null, the error has already been reported
    fprintf(stderr,"%s(%d): Logging failed."ENDL,__FILE__,__LINE__);
}

static
Emitter& operator<<(Emitter& e, aabb_t aabb)
{ size_t ndim;
  int64_t *ori,*shape;
  AABBGet(aabb,&ndim,&ori,&shape);
  e<<BeginMap<<Key<<"ori"<<Value<<Flow<<BeginSeq;
  for(size_t i=0;i<ndim;++i) e<<ori[i];
  e<<EndSeq<<Key<<"shape"<<Value<<Flow<<BeginSeq;
  for(size_t i=0;i<ndim;++i) e<<shape[i];
  return e<<EndSeq<<EndMap;
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
{ for(int i=0;i<nd_id_count;++i)
    if(strcmp(nd_type_to_str((nd_type_id_t)i),str)==0)
      return (nd_type_id_t)i;
  return nd_id_unknown;
}

static
Emitter& operator<<(Emitter& e, ndio_t file)
{ nd_t s=ndioShape(file);
  e<<BeginMap;
  e<<Key<<"type"<<Value<<nd_type_to_str(ndtype(s))
   <<Key<<"dims"<<Value<<Flow<<BeginSeq;
  for(unsigned i=0;i<ndndim(s);++i) e<<ndshape(s)[i];
  e<<EndSeq;
  e<<EndMap;
  ndfree(s);
  return e;
}

static
Emitter& operator<<(Emitter& e, tile_t tile)
{ return e<<BeginMap
   <<Key<<"aabb" <<Value<<TileAABB(tile)
   <<Key<<"shape"<<Value<<TileFile(tile)
   <<EndMap;
}

static
Emitter& operator<<(Emitter& e, pair<const char*,tile_t> path_and_tile)
{ return e<<BeginMap
   <<Key<<"path" <<Value<<path_and_tile.first
   <<Key<<"aabb" <<Value<<TileAABB(path_and_tile.second)
   <<Key<<"shape"<<Value<<TileFile(path_and_tile.second)
   <<EndMap;
}

char *filename(const char *path)
{ char name[]="cache.yaml",
      *out=new char[strlen(path)+sizeof(name)+2],
      *t;
  memset(out,0,strlen(path)+sizeof(name)+2);
  t=strcat(out,path);
  t=strcat(t,PATHSEP);
  t=strcat(t,name);
  return out;
}

//
// === INTERFACE ===
//

#undef LOG
#define LOG(...) fprintf(stderr,__VA_ARGS__)

extern "C"
tilebase_cache_t TileBaseCacheOpen(const char *path, const char *mode)
{ tilebase_cache_t out=0;
  char* fname=0;
  TRY(path && *path);
  TRY(mode && *mode);
  out=new _tilebase_cache_t(path);
  fname=filename(path);
  switch(mode[0])
  { case 'r':
      out->in.open(fname);
      out->mode=MODE_READ;
      TRY(out->in.is_open());
      break;
    case 'w':
      out->out.open(fname,ios::out|ios::trunc);      
      TRY(out->out.is_open());
      out->mode=MODE_WRITE;
      out->emitter<<BeginDoc 
                  <<BeginMap<<Key<<"path" <<Value<<path
                            <<Key<<"tiles"<<Value<<BeginSeq;
      TRY(out->emitter.good());
      break;
    default:
      FAIL("Unrecognized mode.");
  }
  delete[] fname;
  return out;
Error:
  if(out && !out->emitter.good())
    REPORT(!out->emitter.good(),out->emitter.GetLastError().c_str());
  if(fname) delete[] fname;
  TileBaseCacheClose(out);
  return 0;
}

extern "C"
void TileBaseCacheClose(tilebase_cache_t self)
{ if(!self) return;
  switch(self->mode)
  { case MODE_READ:  self->in.close(); break;
    case MODE_WRITE:       
      self->emitter<<EndSeq<<EndMap<<EndDoc;
      if(TileBaseCacheError(self))
        self->emitter<<Comment(TileBaseCacheError(self));
      self->out<<self->emitter.c_str();
      self->out.close(); 
      break;
    default:;
  }
  if(TileBaseCacheError(self))
    REPORT(TileBaseCacheError(self),TileBaseCacheError(self));
  delete self;
}


#undef  LOG
#define LOG(...) log(self,__VA_ARGS__)
 
int handle_subpath(tilebase_cache_t self,tile_t t,const string& root, const Node& n)
{ string sub,full(root);
  n>>sub;
  full+=sub;
  TRY(t->meta=MetadataOpen(full.c_str(),NULL,"r"));
  return 1;
Error:
  return 0;
}
int handle_aabb(tilebase_cache_t self,tile_t t,const Node& n)
{ const Node &nori  =n["ori"],
             &nshape=n["shape"];
  size_t i,ndim=nori.size();
  int64_t *ori,*shape;
  TRY(t->aabb=AABBMake(ndim));
  AABBGet(t->aabb,0,&ori,&shape);
  for(i=0;i<ndim;++i)
  { nori[i]  >>ori[i];
    nshape[i]>>shape[i];
  }
  return 1;
Error:
  return 0;
}
int handle_shape(tilebase_cache_t self, tile_t t,const Node& n)
{ const Node& s=n["dims"];
  size_t ndim=s.size();
  string type;
  nd_type_id_t tid;
  //cout << n["dims"];
  n["type"]>>type;
  TRY((tid=str_to_nd_type(type.c_str()))>nd_id_unknown);
  TRY(ndcast(t->shape=ndinit(),tid));
  for(unsigned i=0;i<ndim;++i)
  { size_t d;
    s[i]>>d;
    ndShapeSet(t->shape,i,d);
  }
  return 1;
Error:
  return 0;
}

tile_t make_tile(tilebase_cache_t self, const string& root, const Node &n)
{ tile_t out=0;
  NEW(struct _tile_t,out,1);
  ZERO(struct _tile_t,out,1);
  TRY(handle_subpath(self,out,root,n["path"]));
  TRY(handle_aabb(self,out,n["aabb"]));
  TRY(handle_shape(self,out,n["shape"]));
  return out;
Error:
  return 0;
}

extern "C"
tilebase_cache_t TileBaseCacheRead(tilebase_cache_t self, tiles_t tiles)
{ TRY(!TileBaseCacheError(self));
  TRY(tiles);
  { Parser parser(self->in);
    Node doc;
    string root;
    size_t n,i;
    TRY(parser.GetNextDocument(doc)); // only do first doc
    doc["path"] >> root;
    const Node& t=doc["tiles"];
    TRY(tiles->tiles=(tile_t*)realloc(tiles->tiles,t.size()*sizeof(tile_t)));
    tiles->sz=tiles->cap=t.size();
    i=0;
    for(Iterator it=t.begin();it!=t.end();++it,++i)
      TRY(tiles->tiles[i]=make_tile(self,root,*it));
  }
  return self;
Error:
  return 0;
}


extern "C"
tilebase_cache_t TileBaseCacheWrite(tilebase_cache_t self, const char*path, tile_t t)
{ TRY(t);
  TRY(!TileBaseCacheError(self));
  self->emitter<<pair<const char*,tile_t>(self->relative(path),t);
  TRY(self->emitter.good());
  return self;
Error:
  if(self && !self->emitter.good())
    REPORT(!self->emitter.good(),self->emitter.GetLastError().c_str());
  TileBaseCacheClose(self);
  return 0;
}

extern "C"
char* TileBaseCacheError(tilebase_cache_t self) 
{return self?self->log:"(void)";}