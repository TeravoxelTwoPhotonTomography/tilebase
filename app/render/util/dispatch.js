#!/bin/env node
/* 
   TODO

   [x] mock: ensure dependencies between batched jobs are correct
   [x] assemble batch command submission
       [x] FIXME: exec is not serializing as desired
   [x] write batch command.  test.
   [ ] throttle
   [ ] scale

*/

// --- prelude --- 

var spawn = require('child_process').spawn,
    map   = require('async').map,
    series= require('async').series,
    foldl = require('async').foldl,
    path  = require('path')

Array.prototype.unique=
function unique() {
  return this.reduce(function(p,c) {
    if(p.indexOf(c)<0) p.push(c);
    return p;
  },[]);
}

// --- scheduling ---

function group_by_depth(addrs,cb) {
  function batch(xs,n){
    var t,r=[];
    while((t=xs.splice(0,n)).length)
      r.push(t);
    return r;
  }

  var d={};
  map(Object.keys(addrs),
      function(v,ret) {var k=v.length;if(v=='0') k=0; if(!d[k]) {d[k]=[v];} else {d[k].push(v);} ret();},
      function() {
        var g={};
        map(Object.keys(d),
            function(v,ret)  {g[v]=batch(d[v],7);ret();},
            function(err,res){cb(g,addrs);}
           );
      }
     );
}

/*
  cmd   [in]        Root command to run. Each argument should be in a list.
                    Example: ['./render','in','out']
  cb    [callback]  Called with a object that uses the addresses as keys.
                    The values are objects with the addreses' parent, and 
                    the holds for the job.
*/ 
function addresses(cmd,cb) {
  cmd.push("--print-addresses");
  var p=spawn(cmd[0],cmd.slice(1),{cwd:process.cwd()});
  var buf="";
  p.stdout.on('data',function(data) {buf+=data});
  p.stderr.on('data',function(data) {console.log("stderr: "+data);});
  p.on('close',function(code) {
    var d={};
    map(buf.split('\n')
           .filter(function(e){return e;}),
        function(v,ret){d[v]={parent:Math.floor(v/10),holds:[]}; ret();},
        function(err,result) {d['0'].parent=null; group_by_depth(d,cb);}
    );
  });
}

/*
  xs    array               f will be mapped serially over the elements of xs
  f     function(x,ret)     f is an async function that will call ret(err,value)
                            when done.
  ret   function(err,value) A callback that will be called when the entire series
                            is finished.

  NOTE: I'm not strictly sure this was necessary.  foldl might have worked fine.
        This gaurantees the approach since it uses series.
*/
function foreach(xs,f,ret) {
  map(xs,
      function(x,r)    { r(null,function(cb) {f(x,cb);}); },
      function(err,fs) { series(fs,ret);}
     );
}

/*
  batches is an object:  {depth:   [ [ addr, addr,... ],...]}
  jobs is an object:     {address: { parent: addr, holds: [jid,...] }}
  submit is a function:  submit(addresses,holds,cb) --> jid (via callback)
                         It assembles the qsub call and returns the job id of the submited job.

                         addresses is [addr,...]
                         holds     is [jid,...]
                         cb        is function(err,jid)
                                      where 
                                        err is not null when there is an error
                                        jid is the job id of the submitted job (see qsub)
  cb is a function:      It is called with no arguments when exec is done.

  exec submits job batches in reverse order of detpth (from highest depth to lowest),
  and adds the job id's to each corresponding parent address as a hold.
*/
function exec(batches,jobs,submit,cb) {
  var depths=Object.keys(batches).sort(function(a,b) {return b-a;});
  foreach(depths,
          function(d,retd) {
            console.log({depth:d});
            foreach(batches[d],
                    function(addrs,retb) {
                      console.log({batch:addrs});
                      foldl(addrs,[],           //1 : aggregate existing holds for addresses
                          function(holds,addr,ret) {
                            ret(null,holds.concat(jobs[addr].holds))
                          },
                          function(err,holds) { //2 : submit jobs
                            enqueue(function(){
                              submit(addrs,holds.unique(),function(err,jid){
                                var i,p;        //3 : update parents (if any)
                                for(i=0;i<addrs.length;++i)
                                  if((p=jobs[addrs[i]].parent)!=null)
                                    jobs[p].holds.push(jid);
                                retb();
                              });
                            });
                          }
                      );
                    },
                    retd);
          },
          function() {stopQstatPolling(); cb();}
         );
}

// --- throttle ---
// gate based on number of qstat'd jobs
// b.c. of the way scheduling is serialized here, there's only 1 job in the internal queue at any time.

var queue=[]
function enqueue(cb) { queue.push(cb); }
function popn(n) {
  for(i=0;i<Math.min(n,queue.length);++i)
    if(cb=queue.shift())
      cb();
}
var poll=setInterval(function(){
  var thresh=200;
  var buf="";
  var p=spawn(path.join(__dirname,'my-job-count.sh'));
  p.stdout.on('data',function(data) {buf+=data});
  p.stderr.on('data',function(data) { console.log('ERROR: '+data);});
  p.on('close',function(code) {
    console.log("Check queue: Size: "+queue.length+" (thresh: "+thresh+") - Jobs: "+buf);
    if(buf<thresh) {popn(thresh-buf);}
  });
},100); // check queue once every 5 seconds
function stopQstatPolling() {clearTimeout(poll);}

// --- job submission ---

function qsubopts(name,log,nbatch,addrs,holds) {
  var holdopt='';
  var batchcmd=[path.join(__dirname,'worker.js '),addrs.join(',')].concat(process.argv.slice(2)).join(' ');
  console.log("QSUBOPTS: holds: "+holds);
  if(holds.length)
    holdopt='-hold_jid '+holds.join(',');
  return ('-terse -V -N '+name+' -j y -o '+log+' -b y -cwd -pe batch '+nbatch+' -l gpu=true '+holdopt).trim().split(' ').concat([batchcmd]);
}

function qsub(addrs,holds,ret) {
  QSUB(qsubopts('clackn-render','/dev/null/',7,addrs,holds),function(r) {console.log(r); ret(null,r);});
}

var njobs=0;
function QSUB(opts,on_jid) {
  var buf='';
  function done() {njobs--; on_jid(parseInt(buf));}
  console.log('['+njobs+']: '+opts);
  var j=spawn('qsub',opts);
  j.on('exit',done); 
  j.stderr.on('data',function(data) {console.log('qsub stderr: '+data.toString());});
  j.stdout.on('data',function(data) {buf+=data.toString();});
  njobs++;
}

// --- MAIN ---

// callback needs to be function(batches,jobs)
addresses(process.argv.slice(2),function(batches,jobs) {
  exec(batches,jobs,qsub,function() {console.log('DONE');});
});

// --- test/debug ---

var mock_jid=0;
function mock(addrs,holds,ret){
  setTimeout(function() {
    console.log([mock_jid,{addrs:addrs,holds:holds}]);
    ret(null,mock_jid++);
  },100);
}
function mockqsub(addrs,holds,ret) {
  setTimeout(function(){
    console.log('qsub '+qsubopts('mbm-render','log2/',7,addrs,holds));
    ret(null,mock_jid++);
  },100);
}
