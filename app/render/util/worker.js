#!/bin/env node

/*
  Usage:
  
    batch.js [addresses] [root command...]

    addresses     A commma seperated list of node addresses in the target octree 
                  Example: 11,12,13,14,15,16,17

    root command  The render command that will be used to target each address.
                  Example: ./render in out -x 1 -y 1 -z 2
*/                

console.log({addrs:process.argv[2],
               cmd:process.argv.slice(3)})
    

