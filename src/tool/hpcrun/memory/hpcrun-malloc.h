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

//***************************************************************************
//
// File:
//    csprof-malloc.h
//
// Purpose:
//    An interface to and implementation of privately managed dynamic
//    memory. This interface enables the profiler to allocate storage  
//    when recording a profile sample inside malloc.
//
//***************************************************************************

#ifndef hpcrun_malloc_h
#define hpcrun_malloc_h

#include "valgrind.h"

//************************* System Include Files ****************************

#include <stddef.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

#if ! defined(VALGRIND) || defined(_IN_MEM_C)

//---------------------------------------------------------------------------
// Function: hpcrun_malloc 
//
// Purpose: return a pointer to a block of memory of the *exact* size 
//      (in bytes) requested.  If there is insufficient memory, an attempt 
//      will be made to allocate more.  If this is not possible, an error 
//      is printed and the program is terminated.
// 
// NOTE: This memory cannot be freed! 
//---------------------------------------------------------------------------
void* hpcrun_malloc(size_t size);
void* hpcrun_malloc_freeable(size_t size);

#else
#define hpcrun_malloc malloc
#define hpcrun_malloc_freeable malloc

#endif // VALGRIND

void hpcrun_memory_reinit(void);
void hpcrun_reclaim_freeable_mem(void);
void hpcrun_memory_summary(void);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // CSPROF_MALLOC_H
