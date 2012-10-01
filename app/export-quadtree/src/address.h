/**
 * \file
 * Describes a path through a tree.
 */
#pragma once
#ifdef __cplusplus
extern "C"{
#endif

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



#ifdef __cplusplus
} //extern "C"
#endif