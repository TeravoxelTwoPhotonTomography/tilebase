var readdir   = require('fs').readdir,
    join      = require('path').join,
    normalize = require('path').normalize,
    queue     = require('async').queue,
    spawn     = require('child_process').spawn;

// === ARGUMENT HANDLING ===
// DEFINES cmd, root

if(process.argv.length!=4)
{ console.log('Usage : node '+__filename+' <task> <rootpath>');
  process.exit(0);
}
cmd  = process.argv[2];
root = process.argv[3];
                
// === SUBMIT JOB WITH DEPENDENCIES ===
var njobs=0;
function job(path,args,callback)
{ 
  var hold = function (holds) { return ((args&&args.length)?(' -hold_jid '+args):' ') } 
  console.log('DO '+path+'\tHOLD '+hold(args)+'\t -->  '+jobid);
  var thing=('-terse -V -N clackn-tilebase-cache -o /dev/null -j y -b y -l short=true -cwd'+hold(args)).trim().split(' ');
  thing.push(cmd+' '+path);
  //console.log(thing);

  var j=spawn('qsub',thing);
  j.stdout.on('data',function(data)
  { callback(null,parseInt(data.toString()));
  });
  njobs++;
}

// === ASYNC DIRECTORY WALKER ===
// for every subdirectory, want a dependency id
//
// callback: return_depid(err,integer)
//
// It's important that return_depid() be called for every
// item that might be pushed to the queue.  This callback
// is sometimes responsible for calling the signal that
// removes a task from the queue, and hence is required to
// gaurantee the queue drains.
//
function walk(path,return_depid)
{ 
  var dependencies = new Array()
  var q=queue(function(subdir,callback)
  { walk(subdir,function(err,depid)
    {  if(!err && depid) dependencies.push(depid);
       callback();
    });
  }, 8);
  q.drain = function() 
  { job(path,dependencies,return_depid);
  }
  readdir(path,function(err,files)
  { if(!err && files.length) 
    { files.forEach(function(v) 
      { q.push(join(path,v));
      });        
    } else
    { return_depid(err);
    }
  });
}

// === Launch ===

walk(normalize(root),function(err,depid)
{ if(err) console.log('DONE (ERROR)')
  else    console.log('DONE: '+depid);
  console.log('COUNT: '+njobs);
});
