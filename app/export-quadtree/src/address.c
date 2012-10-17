/**
 * \file
 * Describes a path through a tree.
 */
#include "address.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENDL        "\n"
#define LOG(...)    fprintf(stderr,__VA_ARGS__) 
#define TRY(e)      do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define NEW(T,e,N)  TRY((e)=malloc(sizeof(T)*(N)))
#define ZERO(T,e,N) memset((e),0,(N)*sizeof(T))
#define REALLOC(T,e,N)  TRY((e)=realloc((e),sizeof(T)*(N)))

struct _address_t
{ int*     ids; // array of child indexes starting from the root of the tree.
  unsigned sz,  // the length of the path
           cap, // the capacity allocated for the \a ids array.  This allows \a ids to be dynamically extended as necessary
           i;   // A cursor ues for iteration that keeps track of where the iterator is on the path.
};

/** The child id at the current iterator position. */
unsigned address_id(address_t self)
{return self->ids[self->i];}

/** Initialize the iterator */
address_t address_begin(address_t self)
{ self->i=0;
  return self->sz?self:0;
}

/** Move the iterator down the tree one step. */
address_t address_next(address_t self)
{ if(!self) return 0;
  ++self->i;
  return (self->i<self->sz)?self:0;
}

/** Construct an address_t object */
address_t make_address()
{ address_t out=0;
  NEW(struct _address_t,out,1);
  ZERO(struct _address_t,out,1);
  return out;
Error:
  return 0;
}

/** Destroy an address_t object */
void free_address(address_t self)
{ if(!self) return;
  if(self->ids) free(self->ids);
  free(self);
}

/** Add a node to the end of the path. */
address_t address_push(address_t self, int i)
{ if(self->sz+1>=self->cap)
    REALLOC(int,self->ids,self->cap=(unsigned)(1.2*self->cap+10));
  self->ids[self->sz++]=i;
  return self;
Error:
  return 0;
}

/** Remove a node from the end of the path. */
address_t address_pop(address_t self)
{ if(self->sz==0) return 0;
  self->sz--;
  return self;
}

/** Render address to an unsigned integer (for printing).*/
uint64_t address_to_int(address_t self, uint64_t base)
{ uint64_t v=0;
  size_t i;
  for(i=0;i<self->sz;++i)
    v=v*base+(1+self->ids[i]); // 1,2,3,4
  return v;
}