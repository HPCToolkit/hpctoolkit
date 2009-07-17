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

#ifndef csprof_malloc_h
#define csprof_malloc_h

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
// Function: csprof_malloc 
//
// Purpose: return a pointer to a block of memory of the *exact* size 
//      (in bytes) requested.  If there is insufficient memory, an attempt 
//      will be made to allocate more.  If this is not possible, an error 
//      is printed and the program is terminated.
// 
// NOTE: This memory cannot be freed! 
//---------------------------------------------------------------------------
void* csprof_malloc(size_t size);
void* csprof_malloc_freeable(size_t size);

#else
#define csprof_malloc malloc
#define csprof_malloc_freeable malloc

#endif // VALGRIND

void hpcrun_reclaim_freeable_mem(void);
void hpcrun_memory_summary(void);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // CSPROF_MALLOC_H
