/**
 * \file
 * Describes a path through a tree.
 */
#pragma once
#ifdef __cplusplus
extern "C"{
#endif
#include <stdint.h>

typedef struct _address_t* address_t;

// construct/destruct
address_t make_address();
void      free_address(address_t self);

// iteratable
unsigned  address_id(address_t self);
address_t address_begin(address_t self);
address_t address_next(address_t self);

// manipulation
address_t address_push(address_t self, int i);
address_t address_pop(address_t self);

// debugging/output
uint64_t address_to_int(address_t self, uint64_t base);

#ifdef __cplusplus
} //extern "C"
#endif