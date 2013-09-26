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
#include <math.h> // for pow
#include "opts.h"
#include "tilebase.h"
#include "src/metadata/metadata.h"

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
  #define strtoll     _strtoi64
  #define PATHSEP    '\\'
  #define S_ISDIR(B) ((B)&_S_IFDIR)
#else
  #define PATHSEP '/'
#endif

// special characters
#define MU "u"

//-- TYPES ---------------------------------------------------------------------
typedef int  (*validator_t)(const char* s);
typedef int  (*parse_t)    (opts_t* ctx,const char* s);
typedef void (*callback_t) (opts_t* ctx);

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
static void help(opts_t *opts);
static int  validate_path(const char* s);
static int  is_valid_output(const char* s);
static int  is_human_readible_size(const char* s); // [ ] TODO
static int  is_double(const char *s);
static int  is_zero_to_one(const char* s);         // [ ] TODO
static int  is_four_or_eight(const char* s);       // [ ] TODO
static int  is_address(const char* s);             // [ ] TODO
static int  is_metadata_fmt(const char* s);        // [ ] TODO
static int  is_positive_int(const char* s);

static void set_print_addresses(opts_t *ctx);
static void set_raveler_output(opts_t *ctx);

static int  set_address(opts_t *ctx,const char *s); // [ ] TODO
static int  set_gpu(opts_t *ctx,const char *s); // [ ] TODO
static int  set_source_path(opts_t *ctx,const char *s);
static int  set_output_path(opts_t *ctx,const char *s);
static int  set_dest_file(opts_t *ctx,const char *s); // [ ] TODO
static int  set_metadata_fmt(opts_t *ctx,const char *s); // [ ] TODO
static int  set_x_um(opts_t *ctx,const char *s); // [ ] TODO
static int  set_y_um(opts_t *ctx,const char *s); // [ ] TODO
static int  set_z_um(opts_t *ctx,const char *s); // [ ] TODO
static int  set_ox(opts_t *ctx,const char *s); // [ ] TODO
static int  set_oy(opts_t *ctx,const char *s); // [ ] TODO
static int  set_oz(opts_t *ctx,const char *s); // [ ] TODO
static int  set_lx(opts_t *ctx,const char *s); // [ ] TODO
static int  set_ly(opts_t *ctx,const char *s); // [ ] TODO
static int  set_lz(opts_t *ctx,const char *s); // [ ] TODO
static int  set_nchildren(opts_t *ctx,const char *s); // [ ] TODO
static int  set_leaf_sz(opts_t *ctx,const char *s); // [ ] TODO
static int  set_fov_x_um(opts_t *ctx,const char *s); // [ ] TODO
static int  set_fov_y_um(opts_t *ctx,const char *s); // [ ] TODO

static int    ARGC;
static char** ARGV;
static opt_t SPEC[]=
{ // val parse cb flag? short long default help {0}
  {NULL,            NULL,            help,                1, "-h",  "--help",           NULL,   "Display this help message.",{0}},
  {NULL,            NULL,            set_print_addresses, 1, "--print-addresses", NULL, NULL,   "Print the addresses of each node in the tree in order of dependency.",{0}},
  {is_address,      set_address,     NULL,                0, "-t",  "--target-address", NULL,   "Render target address in the tree from it's children.",{0}},
  {is_positive_int, set_gpu,         NULL,                0, "-gpu",NULL,               "0",    "Use this GPU for acceleration.",{0}},
  {NULL,            NULL,            set_raveler_output,  1, "--raveler-output", NULL , NULL,   "Save using raveler format.  WORK IN PROGRESS.  Currently just enforces some constraints.",{0}},
  {NULL,            set_dest_file,   NULL,                0, "-f",  "--dest-file",      "default.%.tif",
        "The file name pattern used to save downsampled volumes.  "
        "Different color channels are saved to different volumes.  "
        "For file sereies, put a % sign where you want the color channel id to go.  "
        "Examples:\n"
        "  default.h5    -\tsave to HDF5.\n"
        "  default.%.tif -\tsave to tiff series.\n"
        "  default.%.mp4 -\tsave to mp4 series. One color per file.\n"
        "  default.mp4   -\tsave to mp4.  Will try gray, RGB, RGBA.",{0}},
  {is_metadata_fmt, set_metadata_fmt, NULL,               0, "-F", "--source-format",  NULL,
        "The metadata format used to read the tile database.  "
        "The default behavior is to try to guess the proper format.  ",{0}},
  {is_double,       set_x_um,         NULL,               0, "-x", "--x_um",          "0.5", "Finest pixel size (x "MU"m) to render.",{0}},
  {is_double,       set_y_um,         NULL,               0, "-y", "--y_um",          "0.5", "Finest pixel size (y "MU"m) to render.",{0}},
  {is_double,       set_z_um,         NULL,               0, "-z", "--z_um",          "0.5", "Finest pixel size (z "MU"m) to render.",{0}},
  {is_zero_to_one,  set_ox,           NULL,               0, "-ox", NULL,             "0.0", "",{0}},
  {is_zero_to_one,  set_oy,           NULL,               0, "-oy", NULL,             "0.0", "",{0}},
  {is_zero_to_one,  set_oz,           NULL,               0, "-oz", NULL,             "0.0", "Output box origin as a fraction of the total bounding box (0 to 1).",{0}},
  {is_zero_to_one,  set_lx,           NULL,               0, "-lx", NULL,             "1.0", "",{0}},
  {is_zero_to_one,  set_ly,           NULL,               0, "-ly", NULL,             "1.0", "",{0}},
  {is_zero_to_one,  set_lz,           NULL,               0, "-lz", NULL,             "1.0", "Output box origin as a fraction of the total bounding box (0 to 1).",{0}},
  {is_four_or_eight,set_nchildren,    NULL,               0, "-c", "--nchildren",     "8"  , "Number of tilmes to subdivide at each level of the tree (4 or 8).",{0}},
  {is_human_readible_size,set_leaf_sz,NULL,               0, "-n", "--count-of-leaf", "64M",
        "Maximum size of leaf volume in pixels.  "
        "You may have to play with this number to get everything to fit into GPU memory.  "
        "It's possible to specify this in a using suffixes: k,M,G,T,P,E suffixes "
        "to specify the size in kilobytes, Megabytes, etc.\n"
        "Examples:\n"
        "  128k \n"
        "  0.5G \n",{0}},
  {is_double,       set_fov_x_um,     NULL,               0, "-fov_x_um", NULL,       "-1.0", "If positive, override the size of the field of view for each tile along the x direction.",{0}},
  {is_double,       set_fov_y_um,     NULL,               0, "-fov_y_um", NULL,       "-1.0", "If positive, override the size of the field of view for each tile along the y direction.",{0}},

};

static arg_t ARGS[]= // position based arguments
{ // validator, parse, callback, name, help, {0}
  {validate_path,   set_source_path, NULL, "source-path", "Data is read from this directory.",{0}},
  {is_valid_output, set_output_path, NULL, "output-path","Results are placed in this root directory.  It will be created if it doesn't exist.",{0}},
};

//-- SPEC IMPLEMENTATION -------------------------------------------------------

/** path must exist */
static int validate_path(const char *path)
{ struct stat s={0};
  if(stat(path,&s)<0) return 0;
  return S_ISDIR(s.st_mode);
}

static int is_valid_output(const char* s)
{ //if(s) return validate_path(s);
  return 1; // NULL is ok
}

static int is_positive_int(const char* s)
{ char *end=0;
  long v=strtol(s,&end,10);
  if(*s!='\0' && *end=='\0' && v>=0 )
    return 1;
  return 0;
}

static int  is_human_readible_size(const char* s)
{ const char *suffixes="kMGPTE";
  char *end=0;
  long i,any=0,r=strtol(s,&end,10);
  if( end==0 || end==s ) return 0;
  any=(strlen(end)==0);
  for(i=0;i<countof(suffixes) && !any;++i)
    any |= (*end==suffixes[i]);
  return (r>0) && any;
}
static void set_human_readible_size(const char *s, unsigned *dst) // assumes validated s
{ const char *suffixes="kMGPTE"; // 10^[3,6,9,12,15,18]
  char *end=0;
  long i,r=strtol(s,&end,10);
  for(i=0;i<countof(suffixes);++i)
    if(*end==suffixes[i])
      break;
  *dst=(unsigned) (r*pow(10,3*(i+1)));
}

static int  is_double(const char *s)        { char *end=0; strtod(s,&end); return (end!=s); }
static int  is_zero_to_one(const char* s)   { char *end=0; double d=strtod(s,&end); return (end!=s) && (d>=-0.00001) && (d<=1.00001); }
static int  is_four_or_eight(const char* s) { char *end=0; long r=strtol(s,&end,10); return (end!=s) && ((r==4) || (r==8)); }
static int  is_address(const char* s)
{ if(s) {
    char *end=0;
    long long r=strtoll(s,&end,10);
    address_t a=0;
    if((end==s) || (r<0)) return 0;
    a=address_from_int((uint64_t)r,end-s,10);
    if(a) {free_address(a); return 1;}
    return 0;
  }
  return 1; // null is ok
}

static int  is_metadata_fmt(const char* s)
{ unsigned i;
  if(!s) return 1; // null is ok
  for(i=0;i<MetadataFormatCount();++i)
    if(strcmp(s,MetadataFormatName(i))==0)
      return 1;
  return 0;
}

static int  set_address(opts_t *ctx,const char *s) // assumes validated
{ if(s)
  { char *end=0;
    long long r=strtoll(s,&end,10);
    address_t a=0;
    a=address_from_int((uint64_t)r,end-s,10);
    ctx->target=a;
  } else
  ctx->target=0;
  return 1;
}

static void set_print_addresses(opts_t *ctx) {ctx->flag_print_addresses=1;}
static void set_raveler_output(opts_t *ctx)  {ctx->flag_raveler_output=1;}
static int  set_gpu(opts_t *ctx,const char *s)          {ctx->gpu_id=strtol(s,0,10);                    return 1;}
static int  set_source_path(opts_t *ctx,const char *s)  {ctx->src=s;                                    return 1;}
static int  set_output_path(opts_t *ctx,const char *s)  {ctx->dst=s;                                    return 1;}
static int  set_dest_file(opts_t *ctx,const char *s)    {ctx->dst_pattern=s;                            return 1;}
static int  set_metadata_fmt(opts_t *ctx,const char *s) {ctx->src_format=s;                             return 1;}
static int  set_x_um(opts_t *ctx,const char *s)         {ctx->x_um=strtod(s,0);                         return 1;}
static int  set_y_um(opts_t *ctx,const char *s)         {ctx->y_um=strtod(s,0);                         return 1;}
static int  set_z_um(opts_t *ctx,const char *s)         {ctx->z_um=strtod(s,0);                         return 1;}
static int  set_ox(opts_t *ctx,const char *s)           {ctx->ox=strtod(s,0);                           return 1;}
static int  set_oy(opts_t *ctx,const char *s)           {ctx->oy=strtod(s,0);                           return 1;}
static int  set_oz(opts_t *ctx,const char *s)           {ctx->oz=strtod(s,0);                           return 1;}
static int  set_lx(opts_t *ctx,const char *s)           {ctx->lx=strtod(s,0);                           return 1;}
static int  set_ly(opts_t *ctx,const char *s)           {ctx->ly=strtod(s,0);                           return 1;}
static int  set_lz(opts_t *ctx,const char *s)           {ctx->lz=strtod(s,0);                           return 1;}
static int  set_nchildren(opts_t *ctx,const char *s)    {ctx->nchildren=strtol(s,0,10);                 return 1;}
static int  set_leaf_sz(opts_t *ctx,const char *s)      {set_human_readible_size(s,&ctx->countof_leaf); return 1;}
static int  set_fov_x_um(opts_t *ctx,const char *s)     {ctx->fov_x_um=strtod(s,0);                     return 1;}
static int  set_fov_y_um(opts_t *ctx,const char *s)     {ctx->fov_y_um=strtod(s,0);                     return 1;}




//-- HANDLING ------------------------------------------------------------------
// Given SPEC and ARGS, handling is pretty general.  Could split out to an API


static char *basename(char* argv0)
{ char *r = strrchr(argv0,PATHSEP);
  return r?(r+1):argv0;
}

static void reporter(const char *file,int line,const char* function,const char*fmt,...)
{ va_list ap;
  fprintf(stdout,"At %s(%d) - %s()\n",file,line,function);
  va_start(ap,fmt);
  vfprintf(stdout,fmt,ap);
  va_end(ap);
}

static void user_facing_reporter(const char *file,int line,const char* function,const char*fmt,...)
{ va_list ap;
  fprintf(stdout,"\nProblem parsing aguments:\n");
  va_start(ap,fmt);
  vfprintf(stdout,fmt,ap);
  va_end(ap);
  fprintf(stdout,"\n");
}

/** Prints a two column output where the help string is wrapped in the second
    column.
*/
static void writehelp(int maxwidth,const char* lhs,int width,const char* help)
{ const int margin=2;
  char helpbuf[1024]={0};
  int n,r=(int)strlen(help); // remainder of help text to write
  char t, // temp
      *s, // the line split point: last space in the current range or a newline
      *h=helpbuf; // current pos in help string
  int ts=-1; // tab stop
  memcpy(h,help,r);
  memset(h+r,0,countof(helpbuf)-r);

  n=maxwidth-width-margin;

  do
  { int nl=0;  // newline flag - newlines reset the tab stop and preserve leading whitespace on the next line
    int nn=n-((ts>=0)?ts:0);
    char *tp=0;// tab pointer
    t=h[nn];
    s=h+r;
    if( ((h+nn)<(helpbuf+r)) || strchr(h,'\n') )
    { h[nn]='\0';
      if((s=strchr(h,'\n'))||(s=strrchr(h,' '))) // split the help string at first newline or the last space
      { nl=(*s=='\n');
        *s='\0';
        if(nn<r) h[nn]=t;
        t=' ';
      } else
      { s=h+nn;
      }
    }
    if(tp=strchr(h,'\t')) // search for first tab to set tab stop
    { *tp= ' ';
    }
    PRINT("%-*s%-*s%-*s%s\n",width,lhs,margin,"",((ts>=0)?ts:0),"",h);
    lhs=""; // lhs is blank after first line
    if(tp) ts=tp-h+1;
    h=s;
    *s=t;
    if(!nl)
    { while(*h==' ' && (h-helpbuf)<r) ++h; // advance through white space
    } else
    { ts=-1; // reset the tab stop on a newline
    }
  } while((h-helpbuf)<r);
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

static void help(opts_t *opts)
{ usage(); exit(0);
}

#define CALLBACK(iopt,opts)      if(SPEC[iopt].callback) SPEC[iopt].callback(&opts)
#define VALIDATE(iopt,str)       (SPEC[iopt].validate==NULL || SPEC[iopt].validate(str))
#define PARSE(iopt,opts,str)     (SPEC[iopt].parse==NULL || SPEC[iopt].parse(&opts,str))

#define SAME(a,b) ((b!=0)&&strcmp(a,b)==0)


#undef LOG
#define LOG(...)          user_facing_reporter(__FILE__,__LINE__,__FUNCTION__,__VA_ARGS__)

opts_t parseargs(int *argc, char** argv[], int *isok)
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
        { if(iarg>=(*argc-1))
          { LOG("\tAn argument was found but the value was not.\n\tOption \"%s\".\n",argv[0][iarg]);
            goto Error;
          }
          if(!VALIDATE(iopt,argv[0][iarg+1]))
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
    { LOG("\tPositional argument validation error.\n\tArgument <%s> got \"%s\".\n",ARGS[iopt].name,argv[0][iarg]);
      goto Error;
    }
    if(!PARSE(iopt,opts,argv[0][iarg]))
    { LOG("\tPositional argument parse error.\n\tArgument <%s> got \"%s\".\n",ARGS[iopt].name,argv[0][iarg]);
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
#undef LOG
#define LOG(...)          reporter(__FILE__,__LINE__,__FUNCTION__,__VA_ARGS__)
