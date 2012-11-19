/**
 * \file
 * Describes a path through a tree.
 */
#pragma once
#ifdef __cplusplus
extern "C"{
#endif
#include <stdint.h>
#include <stdlib.h>

typedef struct _address_t* address_t;

// construct/destruct
address_t make_address();
void      free_address(address_t self);
address_t copy_address(address_t self); // returns a newly constructed address

// query
unsigned  address_eq(address_t a, address_t b);

// iteratable
unsigned  address_id(address_t self);
address_t address_begin(address_t self);
address_t address_next(address_t self);

// manipulation
address_t address_push(address_t self, int i);
address_t address_pop(address_t self);

// debugging/output
uint64_t  address_to_int(address_t self, uint64_t base);
address_t address_from_int(uint64_t v, size_t ndigits, uint64_t base);
char*     address_to_path(char* path, int n, address_t address);

#ifdef __cplusplus
} //extern "C"
#endif