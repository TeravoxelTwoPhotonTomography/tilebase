/**
 * options for query-aabb
 *
 * TODO
 * - scripting/config file (via lua)
 * - threshold width on long option names
 *   A long option name's lhs should be printed on it's own line and should 
 *   not effect the lhs column width.
 * - maybe change callback type so encountering an option can effect a state transition
 * - Make SPEC and ARGS settable - hence making the option parsing an API
 */
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "opts.h"

#define MAXWIDTH (78)

#define countof(e)        (sizeof(e)/sizeof(*(e)))
#define PRINT(...)        printf(__VA_ARGS__)
#define LOG(...)          reporter(__FILE__,__LINE__,__FUNCTION__,__VA_ARGS__)
#define REPORT(msg1,msg2) LOG("\t%s\n\t%s\n",msg1,msg2)
#define TRY(e)            do{if(!(e)){REPORT(#e,"Expression evaluated as false"); goto Error;}}while(0)

#ifdef _MSC_VER
  #include <malloc.h>
  #define alloca      _alloca
  #define snprintf    _snprintf
  #define PATHSEP    '\\'
  #define S_ISDIR(B) ((B)&_S_IFDIR)
#else
  #define PATHSEP '/'
#endif

//-- TYPES --------------------------------------------------------------------- 
typedef int  (*validator_t)(const char* s);
typedef int  (*parse_t)    (opts_t* ctx,const char* s);
typedef void (*callback_t) ();

struct opt_state_t
{ int is_found;
  char buf[64];
};

typedef struct _opt_t
{ validator_t validate;
  parse_t     parse;
  callback_t  callback;
  int         is_flag;  
  const char* shortname;
  const char* longname;
  const char* def; // default
  const char* help;
  struct opt_state_t state;
} opt_t;

typedef struct _arg_t
{ validator_t validate;
  parse_t     parse;
  callback_t  callback;
  const char *name;
  const char *help;
  struct opt_state_t state;
} arg_t;

//-- SPEC ----------------------------------------------------------------------
static void help();
static void markls();
static int  validate_coord(const char* s);
static int  validate_path(const char* s);
static int  x(opts_t *ctx,const char *s);
static int  y(opts_t *ctx,const char *s);
static int  z(opts_t *ctx,const char *s);
static int lx(opts_t *ctx,const char *s);
static int ly(opts_t *ctx,const char *s);
static int lz(opts_t *ctx,const char *s);
static int lall(opts_t *ctx,const char *s);
static int path(opts_t *ctx,const char *s);

static int    ARGC;
static char** ARGV;
static opt_t SPEC[]=
{ 
  { NULL,NULL,help,1,"-h","--help",NULL, "Display this help message.",{0}},
  { validate_coord, x,NULL,0,"-x" ,NULL,"0.0","The minimum x position of the box.  Must be between 0 and 1.",{0}},
  { validate_coord, y,NULL,0,"-y" ,NULL,"0.0","The minimum y position of the box.  Must be between 0 and 1.",{0}},
  { validate_coord, z,NULL,0,"-z" ,NULL,"0.0","The minimum z position of the box.  Must be between 0 and 1.",{0}},
  { validate_coord,lx,NULL,0,"-lx",NULL,"1.0","The length of the box along x.  Must be between 0 and 1.",{0}},
  { validate_coord,ly,NULL,0,"-ly",NULL,"1.0","The length of the box along y.  Must be between 0 and 1.",{0}},
  { validate_coord,lz,NULL,0,"-lz",NULL,"1.0","The length of the box along z.  Must be between 0 and 1.",{0}},
  { validate_coord,lall,markls,0,"-l",NULL,NULL,"Set default length.  Must be between 0 and 1. This will cause preceding length options on the command line to be ignored.",{0}},
};

static arg_t ARGS[]= // position based arguments
{ {validate_path,path,NULL,"tilebase-path","Path to a folder containing a \"tilebase.cache.yml\" file or a set of tiles.",{0}},
};

//-- SPEC IMPLEMENTATION -------------------------------------------------------

/** Mark's l options as found so they're default values won't be used */
static void markls()
{ SPEC[4].state.is_found=1;
  SPEC[5].state.is_found=1;
  SPEC[6].state.is_found=1;
}
/** s must be parsable as a double between 0 and 1 */
static int validate_coord(const char* s)
{ char *end=0;
  double d;
  if(!s) return 0;
  d=strtod(s,&end);
  if(end==s)         return 0;
  if(d<0.0 || 1.0<d) return 0;
  return 1;
}

/** path must exist */
static int validate_path(const char *path)
{ struct stat s={0};
  if(stat(path,&s)<0) return 0;
  return S_ISDIR(s.st_mode);
}

static int x (opts_t *ctx,const char *s) { ctx->x =strtod(s,NULL); return 1;}
static int y (opts_t *ctx,const char *s) { ctx->y =strtod(s,NULL); return 1;}
static int z (opts_t *ctx,const char *s) { ctx->z =strtod(s,NULL); return 1;}
static int lx(opts_t *ctx,const char *s) { ctx->lx=strtod(s,NULL); return 1;}
static int ly(opts_t *ctx,const char *s) { ctx->ly=strtod(s,NULL); return 1;}
static int lz(opts_t *ctx,const char *s) { ctx->lz=strtod(s,NULL); return 1;}
static int lall(opts_t *ctx,const char *s) {lx(ctx,s);ly(ctx,s);lz(ctx,s); return 1;}
static int path(opts_t *ctx,const char *s) {ctx->path=s; return 1;}

static char *basename(char* argv0)
{ char *r = strrchr(argv0,PATHSEP);
  return r?(r+1):argv0;
}

//-- HANDLING ------------------------------------------------------------------
// Given SPEC and ARGS, handling is pretty general.  Could split out to an API

static void reporter(const char *file,int line,const char* function,const char*fmt,...)
{ va_list ap;
  fprintf(stdout,"At %s(%d) - %s()\n",file,line,function);
  va_start(ap,fmt);
  vfprintf(stdout,fmt,ap);
  va_end(ap);
}

/** Prints a two column output where the help string is wrapped in the second
    column. 
*/
static void writehelp(int maxwidth,const char* lhs,int width,const char* help)
{ 
  char helpbuf[1024]={0};
  int n,r=(int)strlen(help); // remainder of help text to write
  char t, // temp
      *s, // the line split point: last space in the current range or a newline
      *h=helpbuf; // current pos in help string
  memcpy(h,help,r);
  memset(h+r,0,countof(helpbuf)-r);

  n=maxwidth-width+4;
  t=h[n];
  s=h+r;
  if((h+n)<(helpbuf+r))    // test if the line needs to wrap
  { h[n]='\0';             // null the wrap point
    if((s=strchr(h,'\n'))||(s=strrchr(h,' '))) // split the help string at first newline or the last space
    { *s='\0';
      if(n<r) h[n]=t;
      t=' ';
    } else
    { s=h+n;
    }
  }
  PRINT("%-*s    %s\n",width,lhs,h);
  h=s;
  *s=t;
  while(*h==' ' && (h-helpbuf)<r) ++h; // advance through white space

  while((h-helpbuf)<r)
  {
    t=h[n];
    s=h+r;
    if((h+n)<(helpbuf+r))
    { h[n]='\0';
      if((s=strchr(h,'\n'))||(s=strrchr(h,' '))) // split the help string at first newline or the last space
      { *s='\0';
        if(n<r) h[n]=t;
        t=' ';
      } else
      { s=h+n;
      }
    }
    PRINT("%-*s    %s\n",width,"",h);
    h=s;
    *s=t;
    while(*h==' ' && (h-helpbuf)<r) ++h; // advance through white space
  }
}

static void usage()
{ int i,width=0;
  PRINT("Usage: %s [options]",basename(ARGV[0]));
  for(i=0;i<countof(ARGS);++i)
    PRINT(" <%s>",ARGS[i].name);
  
  for(i=0;i<countof(ARGS);++i)
  { int c=0;
#define WRITE(...) c+=snprintf(ARGS[i].state.buf+c,sizeof(ARGS[i].state.buf)-c,__VA_ARGS__)
    WRITE("    %s",ARGS[i].name);
#undef WRITE
  }
  

  // left column of help text
  for(i=0;i<countof(SPEC);++i)
  { int c=0;
#define WRITE(...) c+=snprintf(SPEC[i].state.buf+c,sizeof(SPEC[i].state.buf)-c,__VA_ARGS__)
    WRITE("    %s",SPEC[i].shortname);
    if(SPEC[i].longname)
      WRITE(" [%s]",SPEC[i].longname);
    if(!SPEC[i].is_flag)
    { WRITE(" arg");
      if(SPEC[i].def)
        WRITE(" (=%s)",SPEC[i].def);
    }
#undef WRITE
  }
  // width of left column
  for(i=0;i<countof(ARGS);++i)
  { int n=(int)strlen(ARGS[i].state.buf);
    width=(n>width)?n:width;
  }
  for(i=0;i<countof(SPEC);++i)
  { int n=(int)strlen(SPEC[i].state.buf);
    width=(n>width)?n:width;
  }

  PRINT("\nArguments:\n");
  for(i=0;i<countof(ARGS);++i)
    writehelp(MAXWIDTH,ARGS[i].state.buf,width,ARGS[i].help);

  PRINT("\nOptions:\n");
  for(i=0;i<countof(SPEC);++i)
    writehelp(MAXWIDTH,SPEC[i].state.buf,width,SPEC[i].help);
  
  printf("\n");
}

static void help()
{ usage(); exit(0); 
}

#define CALLBACK(iopt,opts)      if(SPEC[iopt].callback) SPEC[iopt].callback(&opts)
#define VALIDATE(iopt,str)       (SPEC[iopt].validate==NULL || SPEC[iopt].validate(str))
#define PARSE(iopt,opts,str)     (SPEC[iopt].parse==NULL || SPEC[iopt].parse(&opts,str))

#define SAME(a,b) ((b!=0)&&strcmp(a,b)==0)

opts_t parsargs(int *argc, char** argv[], int *isok)
{ opts_t opts={0};
  int iarg,iopt;
  unsigned char *hit=0;  // flags to indicate an argv was used in option processing
  TRY(argc && argv && isok);
  *isok=0;
  TRY(hit=(unsigned char*)alloca(*argc));
  memset(hit,0,*argc);
  ARGV=(char**)*argv;
  ARGC=*argc;
  for(iarg=1;iarg<*argc;++iarg)
  { for(iopt=0;iopt<countof(SPEC);++iopt)
    { if( SAME(argv[0][iarg],SPEC[iopt].shortname)
        ||SAME(argv[0][iarg],SPEC[iopt].longname ))
      { CALLBACK(iopt,opts);
        hit[iarg]=1;
        if(!SPEC[iopt].is_flag)
        { if(!VALIDATE(iopt,argv[0][iarg+1]))
          { LOG("\tArgument validation error.\n\tOption \"%s\" got \"%s\".\n",argv[0][iarg],argv[0][iarg+1]);
            goto Error;
          }
          if(!PARSE(iopt,opts,argv[0][iarg+1]))
          { LOG("\tArgument parse error.\n\tOption \"%s\" got \"%s\".\n",argv[0][iarg],argv[0][iarg+1]);
            goto Error;
          }
          SPEC[iopt].state.is_found=1;
          iarg++; // consumes an argument
          hit[iarg]=1;
        }
      }
    }
  }
  // apply default value for missing arguments
  for(iopt=0;iopt<countof(SPEC);++iopt)
  { if(!SPEC[iopt].is_flag && !SPEC[iopt].state.is_found)
    { if(SPEC[iopt].def) // if a default exists...
      { if(!VALIDATE(iopt,SPEC[iopt].def))
        { LOG("\tDefault argument failed to validate.\n\tOption \"%s\" got \"%s\".\n",SPEC[iopt].shortname,SPEC[iopt].def);
          goto Error;
        }
        if(!PARSE(iopt,opts,SPEC[iopt].def))
        { LOG("\tDefault argument failed to parse.\n\tOption \"%s\" got \"%s\".\n",SPEC[iopt].shortname,SPEC[iopt].def);
          goto Error;
        }
      }
    }
  }
  // fixed place argument handling and sections
#undef CALLBACK
#undef VALIDATE
#undef PARSE  
#define CALLBACK(iopt,opts)      if(ARGS[iopt].callback) ARGS[iopt].callback(&opts)
#define VALIDATE(iopt,str)       (ARGS[iopt].validate==NULL || ARGS[iopt].validate(str))
#define PARSE(iopt,opts,str)     (ARGS[iopt].parse==NULL || ARGS[iopt].parse(&opts,str))
  for(iarg=1,iopt=0;(iarg<*argc)&&(iopt<countof(ARGS));++iarg)
  { if(hit[iarg]) continue;
    if(!VALIDATE(iopt,argv[0][iarg]))
    { LOG("\tPositional argument validation error.\n\tOption <%s> got \"%s\".\n",ARGS[iopt].name,argv[0][iarg]);
      goto Error;
    }
    if(!PARSE(iopt,opts,argv[0][iarg]))
    { LOG("\tPositional argument parse error.\n\tOption <%s> got \"%s\".\n",ARGS[iopt].name,argv[0][iarg]);
      goto Error;
    }
    ARGS[iopt++].state.is_found=1;
  }
  // The specified fixed place args must be found
  // Not too worried about extra args
  for(iopt=0;iopt<countof(ARGS);++iopt)
  { if(!ARGS[iopt].state.is_found)
    { LOG("\tMissing required positional argument <%s>.\n",ARGS[iopt].name);
      goto Error;
    }
  }
  *isok=1;
  return opts;
Error:
  if(isok) *isok=0;
  usage();
  return opts;
}