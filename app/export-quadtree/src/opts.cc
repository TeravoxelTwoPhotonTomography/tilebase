/**
 * Command line option parsing.
 */
#define NOMINMAX
#ifdef _MSC_VER
#include "dirent.win.h"
#else
#include "dirent.h"
#endif
#include "opts.h"
 opts_t OPTS={0}; // instance the OPTS global variable (declared in opts.h)
#include <iostream>
#include <sstream>
#include <cmath>
#include <boost/program_options.hpp>
#include "tilebase.h"
#include "src/metadata/metadata.h"
#include <algorithm>

#define countof(e) (sizeof(e)/sizeof(*e))

#ifdef _MSC_VER
 #define PATHSEP '\\'
#else
 #define PATHSEP '/'
#endif

using namespace std;
using namespace boost;
using namespace boost::program_options;

//
// === TYPES ===
//

struct ExistingPath
{ string path_;
  ExistingPath() {}
  ExistingPath(const string& s) :path_(s) {}
};
ostream &operator<<(ostream &stream, ExistingPath  s) {return stream<<s.path_;}
istream &operator>>(istream &stream, ExistingPath &s) {return stream>>s.path_;}

struct TileBaseFormat
{ string path_;
  TileBaseFormat() :path_("") {}
  TileBaseFormat(const string& s) :path_(s) {}
  const char* c_str() {return path_.c_str();}
};
ostream &operator<<(ostream &stream, TileBaseFormat  s) {return stream<<s.path_;}
istream &operator>>(istream &stream, TileBaseFormat &s) {return stream>>s.path_;}

struct HumanReadibleSize
{ size_t sz_;
  HumanReadibleSize(): sz_(0) {}
  HumanReadibleSize(string s): sz_(0) {istringstream ss(s); parse(ss);}
  string print()
  { const char suffix[]=" kMGTPE";
    string t;
    ostringstream ss(t);
    const double u=1024.0;
    double e=floor(log(sz_+0.0)/log(u));
    e=std::min(e,(double)countof(suffix));
    ss<<(int)(sz_/(pow(u,e)));
    if(suffix[(unsigned)e]!=' ')
      ss<<suffix[(unsigned)e];
    return ss.str();
  }
  istream& parse(istream& in)
  { string unit("");
    string suffix("kMGTPE");
    double i;
    in>>i;
    if(suffix.find(in.peek()))
      in>>unit;
    init(i,unit);
    return in;
  }
  void init(double e, const string& unit)
  { const float u=1024;
    const string suffix(" kMGTPE");
    if(unit.empty())
      sz_=e;
    else
      sz_=e*pow(u,(float)suffix.find(unit[0]));
  }
};
ostream &operator<<(ostream &stream, HumanReadibleSize  s) {return stream<<s.print();}
istream &operator>>(istream &stream, HumanReadibleSize &s) {return s.parse(stream); }


struct dir
{ DIR *d_;
  dir(const string& s):d_(0) {d_=opendir(s.c_str());}
  ~dir() {if(d_) closedir(d_);}
  bool ok() const {return d_!=NULL;}
};

//
// === VALIDATORS ===
//

static void validate(any& v,const vector<string>& vals,ExistingPath*,int)
{ validators::check_first_occurrence(v);
  const string& s=validators::get_single_string(vals);
  if(dir(s).ok())
    v=any(ExistingPath(s));
  else
    throw validation_error(validation_error::invalid_option_value);
}

static void validate(any& v,const vector<string>& vals,TileBaseFormat*,int)
{ validators::check_first_occurrence(v);
  const string& s=validators::get_single_string(vals);
  for(unsigned i=0;i<MetadataFormatCount();++i)
    if(s.compare(MetadataFormatName(i))==0)
    { v=any(TileBaseFormat(s));
      return;
    }
  throw validation_error(validation_error::invalid_option_value);
}

static void validate(any &v,const vector<string>& vals,HumanReadibleSize*,int)
{ //validators::check_first_occurrence(v);
  const string& o=validators::get_single_string(vals);
  istringstream ss(o);
  int i=-1;
  string s,suffixes(" kMGPTE");
  ss>>i>>s;
  if(i<=0||suffixes.find(s[0])==suffixes.length())
    throw validation_error(validation_error::invalid_option_value);
  v=any(HumanReadibleSize(o));
  return;
}

//
// === HELPERS ===
//
const char* tilebase_format_help_string()
{ static std::string help(
  "The metadata format used to read the tile database.  "
  "The default behavior is to try to guess the proper format.  "
  "Available formats:"
  );
  for(unsigned i=0;i<MetadataFormatCount();++i)
    help+=string("\n-   ")+string(MetadataFormatName(i));
  if(!MetadataFormatCount())
    help+="   (None Found!)";
  return help.c_str();
}

//
// === GLOBALS ===
//

static ExistingPath   g_input_path;
static std::string    g_output_path;
static std::string    g_dst_pattern;
static HumanReadibleSize g_countof_leaf;
static TileBaseFormat g_src_fmt("");
//
// === OPTION PARSER ===
//

static char *basename(char* argv0)
{ char *r = strrchr(argv0,PATHSEP);
  return r?(r+1):argv0;
}

unsigned parse_args(int argc, char *argv[])
{ string usage=string("Usage: ")+string(basename(argv[0]))+" [options] <source-path> <dest-path>";
  options_description cmdline_options("General options"),
                      file_opts      ("File options"),
                      param_opts     ("Parameters");
  OPTS.src=OPTS.dst=0;
  try
  {
    cmdline_options.add_options()
      ("help", "Print this help message.")
      ;
    file_opts.add_options() 
      ("source-path,i",
          value<ExistingPath>(&g_input_path)->required(),
          "Data is read from this directory.  Must exist.")
      ("dest-path,o",
          value<string>(&g_output_path)->required(),
          "Results are placed in this root directory.  Will be created if it doesn't exist.")
      ("dest-file,p",
          value<string>(&g_dst_pattern)->default_value("default.%.tif"),
          "The file name pattern used to save downsampled volumes.  "
          "Different color channels are saved to different volumes.  "
          "Put a % sign where you want the color channel id to go.  "
          "Examples:\n"
          "  default.%.tif\n"
          "  default.%.mp4"
      )
      ("source-format,f",
          value<TileBaseFormat>(&g_src_fmt),
          tilebase_format_help_string())
      ;
    param_opts.add_options()
      ("x_um,x",value<float>(&OPTS.x_um)->default_value(0.5),"Finest pixel size (x µm) to render.")
      ("y_um,y",value<float>(&OPTS.y_um)->default_value(0.5),"Finest pixel size (y µm) to render.")
      ("z_um,z",value<float>(&OPTS.z_um)->default_value(0.5),"Finest pixel size (z µm) to render.")
      ("count-of-leaf,n",
           value<HumanReadibleSize>(&g_countof_leaf)->default_value(HumanReadibleSize("64M")),
           "Maximum size of leaf volume in pixels.  "
           "You may have to play with this number to get everything to fit into GPU memory."
           "It's possible to specify this in a using suffixes: k,M,G,T,P,E suffixes "
           "to specify the size in kilobytes, Megabytes, etc. Examples:\n"
           "  128k\n"
           "  0.5G\n"
           )
      ;

    cmdline_options.add(file_opts)
                   .add(param_opts);

    positional_options_description pd;
    pd.add("source-path",1)
      .add("dest-path",1);
    variables_map v;
    store(command_line_parser(argc,argv)
          .options(cmdline_options)
          .positional(pd)
          .run(),v);
    if(v.count("help"))
      { cout<<usage<<endl<<cmdline_options<<endl;
        return 0;
      }
    notify(v);
  }
  catch(std::exception& e)
  {
    cerr<<"Error: "<<e.what()<<endl<<usage<<endl<<cmdline_options<<endl;
    return 0;
  }
  catch(...)
  {
    cerr<<"Unknown error!"<<endl<<usage<<endl<<cmdline_options<<endl;
    return 0;
  }
  OPTS.src=g_input_path.path_.c_str();
  OPTS.dst=g_output_path.c_str();
  OPTS.dst_pattern=g_dst_pattern.c_str();
  OPTS.countof_leaf=g_countof_leaf.sz_;  
  OPTS.src_format=g_src_fmt.path_.c_str();
  #define show(v) cout<<#v" is "<<v<<endl
  //show(OPTS.dst_pattern);
  #undef show
  return 1;
}