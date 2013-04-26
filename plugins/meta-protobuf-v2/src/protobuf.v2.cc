/**
 * \file
 * Metadata format protobuf-based v0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <string>

#include <fcntl.h>
#include <errno.h>

#include <string.h>
#ifdef _MSC_VER
#include "dirent.win.h"
#include <io.h>
#define snprintf _snprintf
#else
#include <dirent.h>
#endif
#include "microscope.pb.h"
#include "stack.pb.h"
#include "tilebase.h"
#include "src/metadata/metadata.h"
#include "src/metadata/interface.h"

#include <iostream>
#include <Eigen/Dense>
#include <Eigen/Core>
#include <Eigen/Geometry>
using namespace Eigen;
using namespace std;

/// @cond DEFINES
#define PBUFV2_FORMAT_NAME "fetch.protobuf.v2"

//#define DEBUG // if defined, turns on debug output

#define ENDL               "\n"
#define LOG(...)           do{ if(!g_silent) fprintf(stderr,__VA_ARGS__); } while(0)
#define TRY(e)             do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define WARN(e)            do{if(!(e)) { LOG("%s(%d): %s() WARNING"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); }} while(0)
#define FAIL(msg)          do{ LOG("%s(%d): %s()"ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,msg); goto Error;} while(0)
#define TRYMSG(e,msg)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e,msg); goto Error;}} while(0)
#define NEW(type,e,nelem)  TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define ZERO(type,e,nelem) memset((e),0,sizeof(type)*nelem)
#define SAFEFREE(e)        if(e){free(e); (e)=NULL;} 
#define countof(e)         (sizeof(e)/sizeof(*(e)))

#ifdef _MSC_VER
 #define open  _open
 #define close _close
 #define PATHSEP '\\'
#else
 #define PATHSEP '/'
#endif
/// @endcond

typedef fetch::cfg::device::MicroscopeV2 scope_desc_t;
typedef fetch::cfg::data::Acquisition  stack_desc_t;
typedef google::protobuf::Message      desc_t;

//
// GLOBALS
//
/// \todo protect globals with a mutex
static int g_silent=0; ///< Use to turn logging to stderr on/off.

#if 1
static void toggle_silence() { g_silent=!g_silent; }
#else
static void toggle_silence() { }
#endif

//
// CONTEXT
//

static const char* normalize_extension(const char *ext)
{ return (ext[0]=='.')?(ext+1):ext;
}

static std::string pop_path(std::string& p)
{ size_t i=p.rfind('/'),
         j=p.rfind('\\');
  return p.substr((i<j)?j:i);
}

struct pbufv2_t
{ scope_desc_t  scope;
  stack_desc_t  stack;
  unsigned      read_mode,
                write_mode;
  std::string   path;
  
  pbufv2_t(const char* _path):read_mode(0),write_mode(0),path(_path) 
  { // clean path of the trailing path seperator if it's there
    char e= *path.rbegin();
    if(e=='/') // assume unix-style path seperators
      path=path.substr(0,path.size()-1);
  }

  /**
   * Notes:
   * File's have the form:
   * \verbatim
   * <path>/<seriesno>-<prefix>.<channo>.<ext>
   *
   * <channo>   <==> ivol
   * <ext>      <--  scope.stack_extension() as std::string
   * <prefix>   <--  scope.file_prefix() as std::string
   * <seriesno> <--  (complicated) comes from path
   *                 <path> is something like blahblah/%05u/
   *                 Could extract from path
   * \endverbatim
   */
  std::string get_vol_path()
  { //extract the series no. from the path
    std::string ss=pop_path(path);
    const char *series=ss.c_str(),
               *prefix=scope.file_prefix().c_str(),
               *ext=normalize_extension(scope.stack_extension().c_str());
    char buf[1024]={0};
    TRY(snprintf(buf,countof(buf),"%s%s-%s.%%.%s",path.c_str(),series,prefix,ext)>0);
    return string(buf);
Error:
    return string("");
  }
};

//
// HELPERS
//

/** Handles errors during protobuf parsing */
class ErrorCollector:public google::protobuf::io::ErrorCollector
{ 
  int flag;
public:
  ErrorCollector():flag(0) {}
  virtual void AddError(int line, int col, const std::string & message)
    {LOG("Protobuf: Error   - at line %d(%d):"ENDL"\t%s"ENDL,line,col,message.c_str()); flag=1;}
  virtual void AddWarning(int line, int col, const std::string & message)
    {LOG("Protobuf: Warning - at line %d(%d):"ENDL"\t%s"ENDL,line,col,message.c_str());}
  int ok() {return flag==0;}  
};

static
int readDesc(const char* filename, desc_t *desc)
{ int fd;
  int isok=1;
  google::protobuf::io::ZeroCopyInputStream *raw=0;
  google::protobuf::TextFormat::Parser parser;
  ErrorCollector e;
  TRY(-1<(fd=open(filename,O_RDONLY)));
  raw=new google::protobuf::io::FileInputStream(fd);
  parser.RecordErrorsTo(&e);
  TRY(parser.Parse(raw,desc));
  TRY(e.ok());
Finalize:
  if(fd>=0) close(fd);    
  if(raw) delete raw;
  return isok;
Error:
  isok=0;
  goto Finalize;
}

/** 
 * Concatenates a number of strings together.  The constructed string is
 * returned in the preallocated buffer, \a buf.
 */
static void cat(char *buf, size_t nbytes, int nstrings, const char** strings)
{ int i;
  memset(buf,0,nbytes);
  for(i=0;i<nstrings;++i)
    strcat(buf,strings[i]);
}

/** Finds file with extension \a ext at \a path.
*/
static
char* find(char* out,size_t n, const char* path, const char *ext)
{ DIR *dir=0;
  struct dirent *ent=0;
  char *r;
  TRYMSG(dir=opendir(path),strerror(errno));
  while((ent=readdir(dir)))
  { if((r=strrchr(ent->d_name,'.')) && strcmp(r,ext)==0)
    { const char *s[]={path,"/",ent->d_name};
      cat(out,n,countof(s),s);
      break;
    }
  }
  TRYMSG(closedir(dir)>=0,strerror(errno));
  return out;
Error:
  if(dir)
    TRYMSG(closedir(dir)>=0,strerror(errno));
  return NULL;
}

/**
 * Finds the first file in \a path with the extension \ext and
 * uses the protobuf text parser to read a protobuf message \a desc.
 * \returns 1 on success, 0 otherwise.
 */
static
int readDescFromPath(const char* path, const char* ext, desc_t *desc)
{ char name[1024];
  TRY(find(name,sizeof(name),path,ext));
  return readDesc(name,desc); 
Error:
  return 0;
}

/**
 * Get write target.
 * Finds the first file with extension \a ext in \a path.  If
 * none is found, a default name is generated.
 *
 * \see pbufv2_close()
 * \param[out]    out             Output buffer.
 * \param[in]     path            Path to destination directory.
 * \param[in]     ext             The file extension to search for.
 * \param[in]     default_prefix  If no file is found, this will be used to form the returned filename.
 * \returns A string with a destination file path.
 */
static
void getWriteTarget(char* out, size_t n,const char* path, const char* ext, const char* default_prefix)
{ if(!find(out,n,path,ext))
  { const char *p[]={path,"/",default_prefix,ext};
    cat(out,sizeof(out),countof(p),p);
  }
}

static 
int write(const desc_t &msg,const char* path, const char *ext, const char *default_prefix)
{ int fd;
  char out[1024];
  getWriteTarget(out,sizeof(out),path,ext,default_prefix);
  TRY((fd=open(out,O_WRONLY))>-1);
  { google::protobuf::io::FileOutputStream os(fd);
    google::protobuf::TextFormat::Print(msg,&os);
  }
  close(fd);
  return 1;
Error:
  return 0;
}

int parse_mode_string(char* mode, unsigned *r, unsigned *w)
{ *r=*w=0;
  for(char *m=mode;*m;++m)
  { switch(*m)
    { case 'r': *r=1; break;
      case 'w': *w=1; break;
      default: FAIL("Invalid mode string");
    }
  }
  return 1;
Error:
  return 0;
}

//
// INTERFACE
//

const char* pbufv2_name()
{ return PBUFV2_FORMAT_NAME; }

/**
 * Just checks for the existence of the expected files at \a path.
 */
unsigned pbufv2_is_fmt(const char* path, const char* mode)
{ char name[1024];
  scope_desc_t desc;
  unsigned v;
  toggle_silence();
  v=(find(name,sizeof(name),path,".acquisition") && 
          find(name,sizeof(name),path,".microscope") &&
          readDesc(name,&desc));
  toggle_silence();
  return v;
}

/** Valid modes: "r", "w", "rw" */
void* pbufv2_open(const char* path, const char* mode)
{ pbufv2_t *ctx=new pbufv2_t(path);
  TRY(parse_mode_string((char*)mode,&ctx->read_mode,&ctx->write_mode));
  if(ctx->read_mode)
  { TRY(readDescFromPath(path,".microscope",&ctx->scope));
    TRY(readDescFromPath(path,".acquisition",&ctx->stack));
  }
  return (void*)ctx;
Error:
  if(ctx) delete ctx;
  return 0;
}

/** Commit any changes if opened with write mode. */
void pbufv2_close(metadata_t self)
{ pbufv2_t *ctx=(pbufv2_t*)MetadataContext(self);
  if(ctx->write_mode)
  { WARN(write(ctx->scope,ctx->path.c_str(),".microscope" ,"default"));
    WARN(write(ctx->stack,ctx->path.c_str(),".acquisition","default"));  
  }
  delete ctx;
}

/**
 * Returns the location of the tile origin in nanometers.
 *
 * This function is called via MetadataGetOrigin(), which ensures
 * \a self and \a nelem are not NULL.
 */
unsigned pbufv2_origin(metadata_t self, size_t *nelem, int64_t* origin)
{ pbufv2_t *ctx=(pbufv2_t*)MetadataContext(self);
  static const int64_t mm2nm = 1e6;
  if(nelem) *nelem=3;
  if(!origin) //just set nelem and return
    return 1;
  origin[0]=ctx->stack.x_mm()*mm2nm;
  origin[1]=ctx->stack.y_mm()*mm2nm;
  origin[2]=ctx->stack.z_mm()*mm2nm;
  return 1;
}

unsigned pbufv2_set_origin(metadata_t self_, size_t nelem, int64_t* origin)
{ pbufv2_t *self=(pbufv2_t*)MetadataContext(self_);
  double nm2mm=1e-6;
  TRY(nelem==3);
  self->stack.set_x_mm(origin[0]*nm2mm);
  self->stack.set_y_mm(origin[1]*nm2mm);
  self->stack.set_z_mm(origin[2]*nm2mm);
  return 1;
Error:
  return 0;
}

unsigned pbufv2_set_shape (metadata_t self_, size_t nelem, int64_t* shape)
{ pbufv2_t *self=(pbufv2_t*)MetadataContext(self_);
  double nm2um=1e-3;
  TRY(nelem==3);
  self->scope.mutable_fov()->set_x_size_um(shape[0]*nm2um);
  self->scope.mutable_fov()->set_y_size_um(shape[1]*nm2um);
  self->scope.mutable_fov()->set_z_size_um(shape[2]*nm2um);
  return 1;
Error:
  return 0;
}

unsigned pbufv2_shape(metadata_t self, size_t *nelem, int64_t* shape)
{ pbufv2_t *ctx=(pbufv2_t*)MetadataContext(self);
  static const int64_t um2nm = 1e3;
  if(nelem) *nelem=3;
  if(!shape) //just set nelem and return
    return 0;
  shape[0]=ctx->scope.fov().x_size_um()*um2nm;
  shape[1]=ctx->scope.fov().y_size_um()*um2nm;
  shape[2]=ctx->scope.fov().z_size_um()*um2nm;
  return 1;
}

ndio_t pbufv2_get_vol(metadata_t self, const char* mode)
{ pbufv2_t *ctx=(pbufv2_t*)MetadataContext(self);
  return ndioOpen(ctx->get_vol_path().c_str(),"series",mode);
}

// === Transform ===
// The utilities used below don't compose matrices, they just set certain
// elements.

// static void identity(float *matrix, unsigned ndim)
// { unsigned i;
//   memset(matrix,0,sizeof(float)*(ndim+1)*(ndim+1));
//   for(i=0;i<=ndim;++i) matrix[i*(ndim+2)]=1.0f;
// }
// static void setscale(float *matrix, unsigned ndim, unsigned idim, float s)
// { matrix[idim*(ndim+2)]=s;}
// static void flip(float *matrix, unsigned ndim, unsigned idim)
// { matrix[idim*(ndim+2)]*=-1.0f;
// }
// /**
//  * Shear parallel to \a adim by \a bdim:
//  * \verbatim
//  * r'[adim]=r[adim]+s*r[bdim]</tt>
//  * \endverbatim
//  */
// static void shear(float *matrix, unsigned ndim, float s, unsigned adim, unsigned bdim)
// { matrix[adim*(ndim+1)+bdim]=s;
// }
// static void translate(float *matrix, unsigned ndim, float s, unsigned idim)
// { matrix[idim*(ndim+1)+ndim]=s;
// }

unsigned pbufv2_get_transform(metadata_t self, float *transform)
{ pbufv2_t *ctx=(pbufv2_t*)MetadataContext(self);
#if 1
  nd_t vol;  
  unsigned i,n;
  int64_t shape[3],ori[3];

  ndio_t file;
  TRY(file=pbufv2_get_vol(self,"r"));
  vol=ndioShape(file);
  ndioClose(file);

  TRY((n=ndndim(vol))==3 || n==4); // 3d or 3d+1 color dimension
  TRY(pbufv2_shape(self,NULL,shape));
  TRY(pbufv2_origin(self,NULL,ori));

  { MatrixXf center(n+1,n+1),stage(n+1,n+1),S(n+1,n+1),R(n+1,n+1),F(n+1,n+1);

    F.setIdentity().block<3,3>(0,0).diagonal() << -1.0f,-1.0f,1.0f;
    R.setIdentity().block<3,3>(0,0)=AngleAxisf(ctx->scope.fov().rotation_radians(),Vector3f(0,0,1)).matrix();
    center.setIdentity().block<3,1>(0,n)
        << (-(float)ndshape(vol)[0]/2.0f),
           (-(float)ndshape(vol)[1]/2.0f),
           0.0f;
    stage.setIdentity().block<3,1>(0,n)
        << (float)ori[0],
           (float)ori[1],
           (float)ori[2];
    S.setIdentity().block<3,3>(0,0).diagonal()
        <<  shape[0]/(float)ndshape(vol)[0],
            shape[1]/(float)ndshape(vol)[1],
            shape[2]/(float)ndshape(vol)[2];

    Map<Matrix<float,Dynamic,Dynamic,RowMajor> > M(transform,n+1,n+1);
    M.setIdentity();
    M(2,1)=1.0/(float)ndshape(vol)[1]; // shear parallel to z by y: 1 plane along length of 1
    M=(stage              // translate to stage origin      
      *S                  // scale to physical coordinates (and flip)
      *center.inverse()   // move back to stack origin
      *R*F                // rotate and flip field
      *center             // Move origin to optical axis, begining of zpiezo movement
      *M).eval(); // pixels
#ifdef DEBUG
#define show(v) cout<<#v": "<<endl<<(v)<<endl<<endl
    show(R);
    show(F);
    show(center);
    show(stage);
    show(S);
    show(M);
#undef show
#endif
  }
  return 1;
#endif
Error:
  return 0;
}

//
// === EXPORT ===
//

/// @cond DEFINES
#ifdef _MSC_VER
#define shared extern "C" __declspec(dllexport)
#else
#define shared extern "C"
#endif
/// @endcond

shared
const metadata_api_t* get_metadata_api()
{ static const metadata_api_t api =
  {   pbufv2_name,
      pbufv2_is_fmt,
      pbufv2_open,
      pbufv2_close,
      pbufv2_origin,
      pbufv2_set_origin,
      pbufv2_shape,
      pbufv2_set_shape,
      pbufv2_get_vol,
      pbufv2_get_transform,
      ndioAddPlugin,
      NULL
  };
  return &api;
}
