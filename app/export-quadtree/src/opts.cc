/**
 * Command line option parsing.
 */

#include "opts.h"
#include <boost/program_options.hpp>
#include <iostream>
#include "dirent.h"


#include "tilebase.h"
#include "src/metadata/metadata.h"

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
  TileBaseFormat() {}
  TileBaseFormat(const string& s) :path_(s) {}
};
ostream &operator<<(ostream &stream, TileBaseFormat  s) {return stream<<s.path_;}
istream &operator>>(istream &stream, TileBaseFormat &s) {return stream>>s.path_;}

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
      return;
  throw validation_error(validation_error::invalid_option_value);
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

//
// === OPTION PARSER ===
//

static char *basename(char* argv0)
{ char *r = strrchr(argv0,PATHSEP);
  return r?(r+1):argv0;
}

unsigned parse_args(opts_t *opts, int argc, char *argv[])
{ string usage=string("Usage: ")+string(basename(argv[0]))+" [options] <source-path> <dest-path>";
  options_description desc("Options");
  opts->src=opts->dst=0;
  try
  {
    desc.add_options()
      ("help",
          "Print this help message.")
      ("source-path,i",
          value<ExistingPath>(&g_input_path)->required(),
          "Data is read from this directory.  Must exist.")
      ("dest-path,o",
          value<string>(&g_output_path)->required(),
          "Results are placed in this directory.  Will be created if it doesn't exist.")
      ("source-format,f",
          value<TileBaseFormat>(),
          tilebase_format_help_string())
      ;


    positional_options_description pd;
    pd.add("source-path",1)
      .add("dest-path",1);
    variables_map v;
    store(command_line_parser(argc,argv)
          .options(desc)
          .positional(pd)
          .run(),v);
    if(v.count("help"))
      { cout<<usage<<endl<<desc<<endl;
        return 0;
      }
    notify(v);
  }
  catch(std::exception& e)
  {
    cerr<<"Error: "<<e.what()<<endl<<usage<<endl<<desc<<endl;
    return 0;
  }
  catch(...)
  {
    cerr<<"Unknown error!"<<endl<<usage<<endl<<desc<<endl;
    return 0;
  }
  opts->src=g_input_path.path_.c_str();
  opts->dst=g_output_path.c_str();
  return 1;
}