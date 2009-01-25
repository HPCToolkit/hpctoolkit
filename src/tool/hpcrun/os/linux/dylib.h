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

// given a PC 'pc', find the begin address of the procedure and the
// module that contains 'pc'.  Note that the procedure's begin address
// is technically the address of the nearest symbol less than 'pc'
// using dladdr.  Return 0 on success, non-0 on failure.
int 
dylib_find_proc(void* pc, void* *proc_beg, void* *mod_beg);

const char* 
dylib_find_proc_name(const void* pc);

// given a PC, determine if it is within the function that corresponds
// to the bottom (outermost) frame of the process or thread stack.
bool 
dylib_isin_start_func(void* pc);


#endif
