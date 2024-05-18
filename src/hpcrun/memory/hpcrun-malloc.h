// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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


//************************* System Include Files ****************************

#include <stddef.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#ifdef __cplusplus
extern "C" {
#endif

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
void* hpcrun_malloc_safe(size_t size);

void hpcrun_memory_reinit(void);
void hpcrun_reclaim_freeable_mem(void);
void hpcrun_memory_summary(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // CSPROF_MALLOC_H
