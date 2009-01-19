// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef dylib_h
#define dylib_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdlib.h>
#include <stdbool.h>

//*****************************************************************************
// 
//*****************************************************************************

void dylib_map_open_dsos();

void dylib_map_executable();

int dylib_addr_is_mapped(void *addr);

int dylib_find_module_containing_addr(void *addr, 
				      // output parameters
				      char *module_name,
				      void **start, 
				      void **end);

// given a PC, find the PC of the nearest lower symbol (according to
// dladdr)
void* dylib_find_lower_bound(void* pc);

// given a PC, determine if it is within the function that corresponds
// to the bottom (outermost) frame of the process or thread stack.
bool dylib_isin_start_func(void* pc);


#endif
