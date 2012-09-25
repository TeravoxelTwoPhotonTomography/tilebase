/**
 * \file
 * Loader for plugin based tile input and output.
 *
 * \author Nathan Clack
 * \date   June 2012
 */
// \section tileio-plugins-system Plugins
#include "nd.h"
#include "tilebase.h"
#include "metadata.h"
#include "plugin.h"
#include "interface.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(__APPLE__) || defined(__MACH__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#pragma warning( push )
#pragma warning( disable:4996 ) //unsafe function

//-// MACROS: Debugging and Error handling.
/// @cond DEFINES
//#define DEBUG_SEARCH

#define ENDL       "\n"
#define LOG(...)   fprintf(stdout,__VA_ARGS__)
#define TRY(e,msg) do{ if(!(e)) {LOG("%s(%d): %s"ENDL "\tExpression evaluated to false."ENDL "\t%s"ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e,msg); goto Error; }} while(0)
#define NEW(type,e,nelem) TRY((e)=(type*)malloc(sizeof(type)*(nelem)),"Memory allocation failed.")
#define SILENTTRY(e,msg) do{ if(!(e)) { goto Error; }} while(0)
#if 0
#define DBG(...) LOG(__VA_ARGS__)
#else
#define DBG(...)
#endif
#define HERE DBG("HERE %s(%d): %s"ENDL,__FILE__,__LINE__,__FUNCTION__)

//-// MACROS: 
//    Alias the dyld interface to Window's shared library API.
//    Use the dirent (posix) interface for directory traversal.
#ifdef _MSC_VER
#include <windows.h>
#include "Shlwapi.h"                                        // for PathIsRelative()
#include "dirent.win.h"                                     // use posix-style directory traversal
const char* estring();
#else // POSIX
#include <dlfcn.h>
#include <dirent.h>
#define LoadLibrary(name)        dlopen((name),RTLD_LAZY)   // returns NULL on error, use dlerror()
#define FreeLibrary(lib)         (0==dlclose(lib))          // dlclose returns non-zero on error, use dlerror().  FreeLibrary returns 0 on error, otherwise non-zero
#define GetProcAddress(lib,name) dlsym((lib),(name))        // returns NULL on error, use dlerror()
#define estring                  dlerror                    // returns an error string
#define HMODULE                  void*
#endif

#ifdef _MSC_VER
#define EXTENSION "dll"
#elif defined(__APPLE__) && defined(__MACH__)
#define EXTENSION "so" // CMake produces .so for MODULES and .dylib for SHARED.  I assume that's correct.
#else
#define EXTENSION "so"
#endif

/// @endcond

/** Detects loadable libraries based on filename */
static int is_shared_lib(const char *fname,size_t n)
{ const char *dot;
  int len;
  SILENTTRY(dot=strrchr(fname,'.'),"No extension found in file name.");
  len = n-(dot-fname+1);
  DBG("[PLUGIN] Searching for [%10s] Got extension [%15s]. Length %2d. File: %s"ENDL,
      EXTENSION,dot+1,len,fname);
  return len==(sizeof(EXTENSION)-1) //"sizeof" includes the terminating NULL
      && (0==strncmp(dot+1,EXTENSION,len));
Error:
  //LOG("\tFile: %s"ENDL,fname);
  return 0;
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

/**
 * Loads the interface from the shared library named \a fname
 * found at the path \a path.
 */
static metadata_api_t* load(const char *path, const char *fname)
{ metadata_api_t *api=NULL;
  void *lib=NULL;
  get_metadata_api_t get;
#ifdef _MSC_VER
  TRY(SetDllDirectory(path),estring());
  TRY(lib=LoadLibrary(fname),"There was a problem loading the specified library.");
  SetDllDirectory(NULL); // reset
#else
  { char *buf;
    const char *p[]={(char*)path,"/",(char*)fname}; // windows handles the "/" just fine
    size_t n = strlen(path)+strlen(fname)+2; // one extra for the terminating null
    TRY(buf=(char*)alloca(n),"Out of stack space.");
    cat(buf,n,3,p);
    TRY(lib=LoadLibrary(buf),"There was a problem loading the specified library.");
  }
#endif
  DBG("[TRY   ] %-20s fname: %s"ENDL,path,fname);
  SILENTTRY(get=(get_metadata_api_t)GetProcAddress((HMODULE)lib,"get_metadata_api"),estring());
  TRY(api=(metadata_api_t*)get(),fname);
  DBG("[ ---> ] Loaded: %s"ENDL,api->name());
  api->lib=lib;
Finalize:
  return api;
Error:
  if(lib) { FreeLibrary((HMODULE)lib); lib=NULL; }
  goto Finalize;
}

/** A list of loaded plugins. */
typedef struct _apis_t
{ metadata_api_t **v;
  size_t         n,cap;
} apis_t;

/** Adds another loaded plugin, \a fmt, to a resizable list \a a.*/
static int push(apis_t *a, metadata_api_t *fmt)
{ metadata_api_t **v=a->v;
  size_t n          =a->n,
         cap        =a->cap;
  if(!fmt)
    return 1; //ignore with success
  if(n+1>=cap)
    TRY(v=(metadata_api_t**)realloc(v,sizeof(*v)*( cap=(size_t)(1.2*n+10) )),"Expanding format API array.");
  v[n++]=fmt;
  a->v=v;
  a->n=n;
  a->cap=cap;
  return 1;
Error:
  return 0;
}

#if   defined(_MSC_VER)
#include "windows.h"
#elif defined(__MACH__)
#include <mach-o/dyld.h>
#elif defined(__linux)
#error "TODO: implement and test"
#else
#error "Unsupported operating system/environment."
#endif

/**
 * Return the absolute path to the calling executable (the runtime path).
 * The plugin path is resolved relative to this directory.
 * Getting this is strongly system dependendant.
 *
 * I'll add systems as I come across them.
 *
 * The caller must free the returned string.
 *
 * \see http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
 */
static char* rpath(void)
{ char* out=NULL;
#if   defined(_MSC_VER)
  { DWORD sz=1024,ret;
    out=(char*)malloc(sz);
    while( (ret=GetModuleFileName(NULL,out,sz))>sz && ret!=0)
      out=(char*)realloc(out,sz=ret);
    TRY(ret,"Call to GetModuleFileName() failed.");
    *(strrchr(out,'\\'))='\0'; // filename is appended...so trim that off
    return out;
  }
#elif defined(__MACH__)
  { uint32_t bufsize=0;
    char *tmp;
    _NSGetExecutablePath(NULL,&bufsize);
    NEW(char,out,bufsize+1);
    TRY(0==_NSGetExecutablePath(out,&bufsize),"Could not get path to executable.");
    out[bufsize]='\0';
    TRY(tmp=realpath(out,NULL),"Translation to real path failed."); // will heap alloc result
    free(out);
    { char* e=strrchr(tmp,'/');
      if(e) *e='\0';
    }
    return tmp;
  }
#elif defined(__linux)
#error "TODO: implement and test"
#else
#error "Unsupported operating system/environment."
#endif
Error:
  if(out) free(out);
  return NULL;
}

/**
 * Recursively descends a directory tree starting at \a dir searching for 
 * plugins to load.
 *
 * \param[in,out] apis The list of apis found by the search.
 * \param[in]     dir  A pointer to the current directory.
 * \param[in]     path The relative path from the root directory of the seach.
 * \returns 0 on failure, otherwise 1.
 */
static int recursive_load(apis_t *apis,DIR* dir,const char *path)
{ DIR* child=0;
  int is_ok=1;
  struct dirent *ent=0;
  while((ent=readdir(dir))!=NULL)
  { if(ent->d_type==DT_REG
    && is_shared_lib(ent->d_name,strlen(ent->d_name)))
    { TRY(push(apis,load(path,ent->d_name)),"Could not append format API.");
    } else if(ent->d_type==DT_DIR)
    { char *buf;
      size_t n=strlen(path)+strlen(ent->d_name)+2;
      const char *p[]={path,"/",ent->d_name};
      if(ent->d_name[0]=='.')// respect hidden files/paths //0==strcmp(ent->d_name,".")*strcmp(ent->d_name,".."))
        continue;
      TRY(buf=(char*)alloca(n),"Out of stack space.");
      cat(buf,n,3,p);
#ifdef DEBUG_SEARCH
      puts(buf);
#endif
      TRY(child=opendir(buf),buf); // "Failed to open child directory."
      TRY(recursive_load(apis,child,buf),"Search for plugins failed.");
      closedir(child);
      child=0;
    }
  }
Finalize:
  if(child) closedir(child);
  return is_ok;
Error:
  is_ok=0;
  goto Finalize;
}

/**
 * Recursively descends a directory tree starting at \a path searching for 
 * plugins to load.
 *
 * This function is used internally by the ndio libarary to load plugins from
 * a specific location.  It is exposed as part of the ndio library interface,
 * just in case you wanted to load plugins from another location.  However,
 * that's not possible at present.  A function to register a set of plugins
 * loaded by the user needs to be added.
 *
 * \todo Add MetadataRegisterPlugins().
 *
 * \param[in]     path The path to the plugins folder.
 * \param[out]    n    The number of elements in the returned array.
 * \returns 0 on failure, otherwise an array of loaded plugin interfaces.  
 *          The caller is responsible for calling ndioFreePlugins() on the
 *          result when done with the array.
 * \ingroup ndioplugins
 */
metadata_apis_t MetadataLoadPlugins(const char *path, size_t *n)
{ apis_t apis = {0};
  DIR*           dir;
  char *buf=0,
       *exepath=rpath();
  { size_t n=strlen(exepath)+strlen(path)+2; // +1 for the directory seperator and +1 for the terminating null
    const char *p[]={exepath,"/",path};
    TRY(buf=(char*)alloca(n),"Out of stack space.");
    cat(buf,n,3,p);
  }
  TRY(dir=opendir(buf),strerror(errno));
  TRY(recursive_load(&apis,dir,buf),"Search for plugins failed.");
  *n=apis.n;
  
  // Register loaded ndio plugins with loaded metadata plugins
  // to share across shared library boundaries.
  { size_t i,n;
    ndio_fmt_t **ndio_plugins=ndioPlugins(&n);
    for(i=0;i<apis.n;++i)
    { size_t j;
      if(apis.v[i]->add_ndio_plugin)
        for(j=0;j<n;++j)
          apis.v[i]->add_ndio_plugin(ndio_plugins[j]);
    }
  }

Finalize:
  if(dir) closedir(dir);
  if(exepath) free(exepath);
  return apis.v;
Error:
  if(n) *n=0;
  apis.v=NULL;
  goto Finalize;
}

/**
 * Releases resources acquired to load plugins and frees the array.
 * \ingroup ndioplugins
 */
void MetadataFreePlugins(metadata_apis_t fmts, size_t n)
{ size_t i;
  if(!fmts) return;
  for(i=0;i<n;++i)
  { if(fmts[i] && fmts[i]->lib)
      TRY(FreeLibrary((HMODULE)fmts[i]->lib),estring());
Error:
    ; // keep trying to free the others
  }
  free(fmts);
}

#ifdef _MSC_VER
/** \todo make thread safe...add a mutex */
static
const char* estring()
{ static char buf[4096];
  FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &buf,
      sizeof(buf)/sizeof(TCHAR), NULL);
  return buf;
}
#endif

#pragma warning( pop )
