var stat    = require('fs').stat,
    readdir = require('fs').readdir,
    join    = require('path').join,
    relative= require('path').relative,
    mkdirp  = require('mkdirp'),
    spawn   = require('child_process').spawn;

function isdir(path,callback) { 
  // callback(bool) - called with true if path is directory, otherwise false.
  stat(path,function(e,s){if(e) callback(false); else callback(s.isDirectory()); })
}

var njobs=0;

function throttle(n,cb) {
  if(njobs<n) {
    njobs++;
    cb(function() {njobs--;})
  } else {
    setTimeout(function() {throttle(n,cb);},100); // try submitting again in 100 ms
  }
}

function submit(cmd,ondone) {
  console.log('['+njobs+']: '+cmd);
  var j=spawn('qsub',cmd.trim().split(' '));
  j.on('exit',ondone); 
  j.stdout.on('data',function(data) {console.log(data.toString());});
  j.stderr.on('data',function(data) {console.log(data.toString());});
}

function walk(src,dst) {
  isdir(src,function(isdir_) {
    if(isdir_) {
      readdir(src,function(err,entries) {
        if(err) return;
        entries.forEach(function(e,i,v) {
          walk(join(src,e),join(dst,e));
        });
      });
    } else {      // file
      // no log files
      var cmd='-terse -V -N clackn-copy -o /dev/null -j y -b y -l short=true -wd '+process.argv[2]+' cp --parents '+relative(process.argv[2],src)+' '+process.argv[3]
      // log files in script directory
      //var cmd='-terse -V -N clackn-copy -o '+__dirname+' -j y -b y -l short=true -wd '+process.argv[2]+' cp --parents '+relative(process.argv[2],src)+' '+process.argv[3]
      throttle(10,function(ondone) {
        submit(cmd,ondone)
      });
    }
  });
}

// === ARGUMENT HANDLING ===
if(process.argv.length!=4)
{ console.log('Usage : node '+__filename+' <src> <dst>');
  process.exit(0);
}
console.log("SRC: "+process.argv[2])
console.log("DST: "+process.argv[3])

mkdirp(process.argv[3],function(err) {
  if(err) console.log(err);
  else walk(process.argv[2],process.argv[3]);
})

/*
Strategy:

   use: cp --parents  a/b/c dst
        Copies a/b/c to dst/a/b/c and creates any missing intermediate directories.
   see: info cp
        may be gnu specific.  probably won't work on osx, but we don't care.

*/
