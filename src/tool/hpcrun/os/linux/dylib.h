// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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
