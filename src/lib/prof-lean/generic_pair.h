/*
 * generic_pair.h
 *
 *      Author: dxnguyen
 */

#ifndef __GENERIC_PAIR_H__
#define __GENERIC_PAIR_H__

//******************************************************************************
// local include files
//******************************************************************************
#include "mem_manager.h"
#include "generic_val.h"

//******************************************************************************
// macro
//******************************************************************************

#define MAX_GENERICPAIR_STR 65536

//******************************************************************************
// type
//******************************************************************************

typedef struct generic_pair_s {
  void* first;
  void* second;
} generic_pair_t;

//******************************************************************************
// Constructor
//******************************************************************************

generic_pair_t*
generic_pair_t_new(void* first, void* second, mem_alloc m_alloc);


void
generic_pair_t_tostr(generic_pair_t* gen_pair,
					  val_tostr first_tostr, char firststr[],
					  val_tostr second_tostr, char secondstr[], char str[]);

#endif /* __GENERIC_PAIR_H__ */
