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

#ifdef _MSC_VER
#pragma warning (disable:4996) // deprecation warning for sprintf
#define PATHSEP  '\\'
#else
#define PATHSEP  '/'
#endif

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
/** \returns 1 if a and b represent the same address, otherwise 0. */
unsigned  address_eq(address_t a, address_t b)
{ unsigned i;
  if(a->sz!=b->sz) return 0;
  for(i=0;i<a->sz;++i)
    if(a->ids[i]!=b->ids[i])
      return 0;
  return 1;
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

/** Make a copy of an address_t object */
address_t copy_address(address_t self)
{ address_t it,out=0;
  if(!self) return 0;
  TRY(out=make_address());
  for(it=address_begin(self);it;it=address_next(self))
    address_push(out,address_id(it));
  return out;
Error:
  free_address(out);
  return 0;
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

address_t address_from_int(uint64_t v, size_t ndigits, uint64_t base)
{ size_t i;
  address_t out=0;
  TRY(out=make_address());
  for(i=0;i<ndigits;++i)
  { TRY(address_push(out,v%base));
    v/=base;
  }
  return out;
Error:
  free_address(out);
  return 0;
}

/**
 * Renders address to something like "1/2/0/0/4/"
 * \param[in] path    Buffer into which to render the string.
 * \param[in] n       size of the buffer \a path in bytes.
 * \param[in] address The address to render.
 */
char* address_to_path(char* path, int n, address_t address)
{ unsigned k;
  char *op=path;
  for(address=address_begin(address);address&&n>=0;address=address_next(address))
  { k=snprintf(path,n,"%u%c",address_id(address),PATHSEP);
    path+=k;
    n-=k;
  }
  // remove terminal path separator
  if(path!=op) path[-1]='\0';
  return (n>=0)?op:0;
}