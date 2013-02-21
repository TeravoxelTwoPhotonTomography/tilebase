var readdir   = require('fs').readdir,
    stat      = require('fs').stat,
    join      = require('path').join,
    normalize = require('path').normalize,
    mapLimit  = require('async').mapLimit,
    filter    = require('async').filter,
    spawn     = require('child_process').spawn;

// === RECURSIVE DIRECTORY DECENT WITH DEPENDENCY TRACKING ===
function isdir(path,callback) { stat(path,function(e,s){if(e) callback(false); else callback(s.isDirectory()); })}

function walk(root,job)
{ function walk_(path,callback) // walk_ has to have a specific function signature
  { function done(err,dependencies)  { if(err) callback(err); else job(path,dependencies,callback); }
    function wrap(f) { return function (a,cb) {f(join(path,a),cb);} }
    readdir(path,function(err,files) {
      if(err) return; // usually err bc path is not a directory
      console.log('CHECK '+path)      
      filter(files,wrap(isdir),function(subdirs){
        mapLimit(subdirs,2,wrap(walk_),done); // limit to avoid opening too many file handles when spawning sub-processes
      });
    })
  }
  walk_(root,function(){})
}
                
// === SUBMIT JOB WITH DEPENDENCIES ===
var njobs=0;
function qsubjob(cmd)
{ return function (path,args,callback)
  { var hold = function (holds) { return ((args&&args.length)?(' -hold_jid '+args):' ') } 
    console.log('DO '+path+'\tHOLD '+hold(args));
    var thing=('-terse -V -N clackn-tilebase-cache -o /dev/null -j y -b y -l short=true -cwd'+hold(args)).trim().split(' ');
    thing.push(cmd+' '+path);
    var j=spawn('qsub',thing);
    j.stdout.on('data',function(data)
    { callback(null,parseInt(data.toString()));
    });
    njobs++;
  }
}

function mockjob(cmd) // for testing/debugging
{ return function(path,dependencies,callback)
  { njobs++;
    console.log('MOCKJOB '+path+' --> '+njobs)
    console.log('         CMD ',cmd)
    console.log('         DEPENDS ',dependencies)
    callback(null,njobs);
  }
}

// === ARGUMENT HANDLING ===
if(process.argv.length!=4)
{ console.log('Usage : node '+__filename+' <task> <rootpath>');
  process.exit(0);
}
walk(normalize(process.argv[3]),qsubjob(process.argv[2]))

