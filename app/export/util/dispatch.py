#!/bin/env python
"""
Parse addresses from export-quadtree and dispatch cluster jobs with the correct
dependencies.

address     A string like '0' or '1134413' That describes a path to a node in
            the tree. 0 is the root. 1 is the first child of the root. 12 is the 
            second child of the target-groupfirst child.  etc...
addresses   An iterable with an address for each element.
"""

import sys

do_only_first_group=0
disable_qsub=0

### PYTHON24 ADAPT ###
def ifthen(predicate,a,b): 
  if predicate:
    return a
  else:
    return b

### COMMAND LINE GENERATING FUNCTIONS ###

GROUP_COMMAND=lambda addresses:["python",__file__]+sys.argv[1:]+['--target-group']+addresses
def QSUB_COMMAND(addresses,holds):
  holdopt=lambda holds: ifthen(holds,['-hold_jid',','.join(map(str,holds))],[])
  return 'qsub -terse -V -N clackn-mousebrain-export -j y -o /dev/null -b y -cwd -pe batch 7 -l gpu_nodes'.split()+holdopt(holds)+[' '.join(GROUP_COMMAND(addresses))]
def NODE_COMMAND(index,address):
  rootcmd=sys.argv[1:sys.argv.index('--target-group')]
  return rootcmd+['--target-address',address,'--gpu',str(index)]

### UTILITIES ###

def intersect(a,b):
  return list(set(a)&set(b))

def exec_get_addresses():
  '''returns list of addreses'''
  from subprocess import Popen,PIPE
  print __file__
  print sys.argv[1:]
  e=Popen(sys.argv[1:]+['--print-addresses'],stdout=PIPE,stderr=sys.stderr)
  return [x for x in e.stdout]

def exec_group(addresses,holds):
  '''
  returns a job id
  dependencies should be job id's to hold on
  This script "calls" itself to run a job that processes a group of addresses.
  '''
  from subprocess import Popen,PIPE
  print '---'
  print 'ADDRESSES',addresses
  print 'HOLDS    ',holds
  sys.stdout.flush()
  print ' '.join(QSUB_COMMAND(addresses,holds))
  if disable_qsub:
    return 12345
  else:
    e=Popen(QSUB_COMMAND(addresses,holds),stdout=PIPE,stderr=sys.stderr)
    for x in e.stdout:
      return x


### SCHEDULERS ###

class _linear_scheduler:
  def __init__(self):
    self._cid=0;
    self._queue={}
  def submit(self,item,dependencies):
    self._cid+=1
    #print '%10s JOB(%-10u) %s'%(item,self._cid,str(dependencies))
    self._queue[self._cid]={"item":item,"dependencies":dependencies}
    return self._cid

class _group_scheduler:
  def __init__(self,n=4):
    self._cid=0;
    self._queue={}
    self._n=n
  def _deps(self,key): #recursively gather dependencies (memoized for speed)
    return self._queue[key]["ext"]
  def submit(self,item,dependencies):
    key=None
    g=None
    #search for an available group
    d=[]
    for k in dependencies:
      d.extend(self._deps(k))
    d=set(d)
    for k,v in self._queue.iteritems():
      if len(v["items"])<self._n and not k in d:
        key,g=k,v
        break
    if g is None: #new group      
      self._cid+=1
      self._queue[self._cid]={"items":[item],"dependencies":list(set(dependencies)),"ext":list(set([self._cid])|d)}
      return self._cid
    else: #add to group
      g["items"].append(item)
      g["dependencies"]=list(set(g["dependencies"])|set(dependencies))
      g["ext"]=list(set(g["ext"])|d)
      return key
  def dispatch(self):
    q=self._queue.copy()
    def holds(k):
      return reduce(lambda a,b:a+[b],map(lambda kk:q[kk].get("jid",[]),q[k]["dependencies"]),[])
    def launch(k):
      q[k]["jid"]=exec_group(q[k]["items"],holds(k))
      return q[k]["jid"]
    if do_only_first_group:
      print launch(sorted(q.keys())[0])
    else:
      print map(launch,sorted(q.keys()))

  def toposort(self):
    '''iterate over jobs in topological order

    The way the queue gets build, the ordering of the strict ordering of the keys
    should give a topological order over the dependency graph.

    This function provides a means of double-checking.
    '''
    # search for items on which no one depends
    deps=set()
    q=self._queue.copy()
    for v in q.itervalues():
      deps=deps|set(v["dependencies"])
    roots=set(k for k in q.iterkeys() if not k in deps)
    # sort
    def get(k):
      if not q[k].get("visited",False):
        q[k]["visited"]=True
        for i in q[k]["dependencies"]:
          for e in get(i):
            yield e
        yield k
    for k in roots:
      for e in get(k):
        yield e

### JOB ###

class Job:
  def __init__(self,scheduler,jid=None,deps=None):
    self._jobid       =jid
    self._dependencies=ifthen(deps,deps,{}) #deps if deps else {}
    self._scheduler   =scheduler
  def _deps(self):
    return list(set([v._jobid for v in self._dependencies.itervalues() if v._jobid is not None]))
  def iteritems(self):
    return self._dependencies.iteritems();
  def __str__(self):
    return "Job(%s)"%str(self._jobid)+ str(self._deps())
  def submit(self,address):
    a=ifthen(address,address,'0') #address if address else '0'
    self._jobid=self._scheduler.submit(a,self._deps());
    
### JOBTREE ###
class JobTree:
  def __init__(self,addresses,scheduler):
    self.tree=Job(scheduler)
    for line in addresses: #sorted(addresses,lambda a,b: ifthen(len(a)==len(b),cmp(a,b),cmp(len(a),len(b))),reverse=True):
      s=self.tree;
      if(line.strip()=="0"):
        continue
      for c in line.strip():
        t=s._dependencies.get(c,Job(scheduler))
        s._dependencies[c]=t
        s=t
  def iterjobs(self,address='',job=None): 
    if not job:
      job=self.tree
    for k,v in job.iteritems():
      for a,j in self.iterjobs(address+k,v):
        yield a,j
    yield address,job
  def submit(self,address='',job=None):
    addr_cmp=lambda a,b: ifthen(len(a)==len(b),cmp(a,b),cmp(len(a),len(b)))
    pair_cmp=lambda (a0,j0),(a1,j1): addr_cmp(a0,a1)
    for address,job in sorted(self.iterjobs(),pair_cmp,reverse=True):
      print address,job
      job.submit(address)
    return self
  def dispatch(self):
    return self.tree._scheduler.dispatch()

### MAIN ###

if __name__=='__main__':
  from pprint import pprint
  if '--target-group' in sys.argv:
    ### Targets each individual address in a group to one gpu
    import random
    from subprocess import Popen
    addresses=sys.argv[sys.argv.index('--target-group')+1:]
    for c in [' '.join(NODE_COMMAND(i,a)) for i,a in enumerate(addresses)]:
      print>>sys.stderr,c
    subjobs=[Popen(NODE_COMMAND(i,a)) for i,a in enumerate(addresses)]
    print>>sys.stdout,random.randint(0,9999999) #simulates qsub's job id return
    for j in subjobs: # WAIT FOR CHILD PROCESSES TO TERMINATE
      j.wait()
  else:
    ### Grabs addresses and schedules jobs
    GroupScheduler =_group_scheduler()
    JobTree(exec_get_addresses(),GroupScheduler).submit().dispatch()
    print 'SUBMITTED'
    print 'COUNTS'
    hist={}
    for v in GroupScheduler._queue.itervalues():
      k=len(v["items"])
      c=hist.get(k,0)
      hist[k]=1+c
    pprint(hist)
    #print(list(GroupScheduler.toposort()))
