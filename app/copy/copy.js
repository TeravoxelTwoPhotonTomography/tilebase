var stat    = require('fs').stat,
    readdir = require('fs').readdir,
    join    = require('path').join,
    relative= require('path').relative,
    mkdirp  = require('mkdirp'),
    spawn   = require('child_process').spawn,
    async   = require('async')

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
    setTimeout(function() {throttle(n,cb);},3000); // try submitting again in <delay> ms
  }
}

function submit(cmd,ondone) {
  console.log('['+njobs+']: '+cmd);
  var j=spawn('qsub',cmd.trim().split(' '));
  j.on('exit',ondone); 
  j.stdout.on('data',function(data) {console.log(data.toString());});
  j.stderr.on('data',function(data) {console.log(data.toString());});
}

function mock(cmd,ondone) {
  console.log(cmd);
  ondone();
}

function walk(src,dst) {
  isdir(src,function(isdir_) {
    if(isdir_) {
      readdir(src,function(err,entries) {
        if(err) return;

        async.any(entries,
                  function(e,cb) {isdir(join(src,e),cb);},
                  function(isbranch) {
          if(isbranch) { // recursive copy of leaf dir
            entries.forEach(function(e,i,v) {
              walk(join(src,e),join(dst,e));
            });
          } else {
            var tgt = relative(process.argv[2],src);
            var dst = join(process.argv[3],tgt);
            var out = "/dev/null" // __dirname
            var cmd='-terse -V -N clackn-copy -o '+out+' -j y -b y -l archive=true -wd '+process.argv[2]+' mkdir -p '+dst+' && cp -r '+src+' '+join(dst,'..');
            throttle(10,function(ondone) {
              //mock(cmd,ondone);
              submit(cmd,ondone)
            });
          }
        });
      });
    } else {      // file
      console.log("!!! Not Copying: "+src); // get here for cache files in branches
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

