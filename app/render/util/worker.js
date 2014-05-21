#!/bin/env node

/*
  Usage:

    batch.js [addresses] [root command...]

    addresses     A commma seperated list of node addresses in the target octree
                  Example: 11,12,13,14,15,16,17

    root command  The render command that will be used to target each address.
                  Example: ./render in out -x 1 -y 1 -z 2

                  Note that the -gpu and --target-address will be overridden
                  for each gpu specific job.

                  The way arguments are overridden relies on the option parsing
                  behavior in render; arguments at the end of the command line
                  override those that come earlier.
*/

var spawn = require('child_process').spawn,
    map   = require('async').map;

var RENDER='/groups/mousebrainmicro/mousebrainmicro/Software/env/gpu/bin/render';

var batch={
  addrs:process.argv[2],
    cmd:process.argv.slice(4)
};
var log={stdout:{},stderr:{}};
batch.cmd.unshift(RENDER);
var bs=batch.addrs.split(',');
map(bs,
    function(addr,ret) {
      var idx = bs.indexOf(addr);
      var cmd=batch.cmd.concat(['-gpu',idx,'--target-address',addr]);
      var p=spawn(cmd[0],cmd.slice(1),{cwd:process.cwd()});
      var isdone=0;
      function done() { if(!isdone) {isdone=1; hostname(ret);}}
      log.stdout[addr]='';
      log.stderr[addr]='';
      p.on('exit',function(code) {console.log(log.stdout[addr]); console.error(log.stderr[addr]);done();});
      p.on('error',function(err) {console.error("!!! Worker Spawn Error: "+{code:err,addr:addr});done();});
      p.stdout.on('data',function(data) {log.stdout[addr]+=data.toString();});
      p.stderr.on('data',function(data) {log.stderr[addr]+=data.toString();});
    },
    function(err,res) {
      console.log('BATCH DONE');
    }
);

function hostname(cb) {
  var buf='';
  var isdone=0;
  var done=function() {if(!isdone) {isdone=1; cb();}};
  var p=spawn('hostname');
  p.stdout.on('data',function(data) {buf+=data;});
  p.stderr.on('data',function(data) {buf+=data;});
  p.on('close',function(code) {console.log('HOSTNAME: '+ buf); done()});
  p.on('error',function(error) {console.log('HOSTNAME: Error - ' + {code:code, buf:buf}); done()});
}
