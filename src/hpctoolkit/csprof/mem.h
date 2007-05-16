// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002-2007, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    csprof_mem.h
//
// Purpose:
//    An interface to and implementation of privately managed dynamic
//    memory as an alternative to malloc.  The memory is implemented
//    using memory maps that use the system's swap space for backing
//    (in contrast to a file or some other shared device).  When space
//    in one pool of dynamic store becomes low, another pool will be
//    allocated if possible.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSPROF_MEM_H
#define CSPROF_MEM_H

//************************* System Include Files ****************************

#include <stddef.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned long offset_t;

//***************************************************************************
// helper structures for csprof_mem: these structures are useful
// containers of data and are not designed to manipulate their own
// data.
//***************************************************************************

// ---------------------------------------------------------
// csprof_mmap_info_t: Represents information about a mmap (with swap
// space backing) used to implement a private memory store. (We assume
// the filename is either not applicable or something like /dev/zero.)
// 
// Note: This structure should always be placed at the beginning of the
// mmap it describes.
// ---------------------------------------------------------
typedef struct csprof_mmap_info_s {

  void*    beg; // start address
  offset_t sz;  // size (in bytes) (beg + sz = end)
  int      fd;  // file descriptor of assoc. file (-1 for none/invalid)

  struct csprof_mmap_info_s* next; // list

} csprof_mmap_info_t;

int csprof_mmap_info__init(csprof_mmap_info_t* x);
int csprof_mmap_info__fini(csprof_mmap_info_t* x);


// ---------------------------------------------------------
// csprof_mmap_alloc_info_t: Store information about memory allocation
// in 'mmap'
// ---------------------------------------------------------
typedef struct csprof_mmap_alloc_info_s { 

  csprof_mmap_info_t* mmap; // mmap to allocate memory in

  void* next; // start of next free block in 'mmap'
  void* beg;  // min address in 'mmap'
  void* end;  // max address in 'mmap' (unavailable for storage)

} csprof_mmap_alloc_info_t;

int csprof_mmap_alloc_info__init(csprof_mmap_alloc_info_t* x);
int csprof_mmap_alloc_info__fini(csprof_mmap_alloc_info_t* x);

//***************************************************************************
// csprof_mem: (quasi-class)
//***************************************************************************

typedef enum csprof_mem_status_e {
  CSPROF_MEM_STATUS_UNINIT = 0, /* mem has not been initialized */
  CSPROF_MEM_STATUS_INIT,       /* mem has been initialized */
  CSPROF_MEM_STATUS_FINI        /* mem has been finalized (after init) */
} csprof_mem_status_t;

typedef enum csprof_mem_store_e {
  CSPROF_MEM_STORE,
  CSPROF_MEM_STORETMP 
} csprof_mem_store_t;

// ---------------------------------------------------------
// csprof_mem_t: Collects together the data structures used to
// implement private memory management.  When memory in one store
// (mmap) runs low, another will be allocated, if possible to satisfy
// demand.  
//
// Note: This structure itself will be stored in static memory, but
// each csprof_mmap_info_t structure in a 'store' list is placed at
// the beginning of the mmap that it refers to.
// ---------------------------------------------------------
typedef struct csprof_mem_s {

  // A list of mmaps and alloc info for regular storage
  csprof_mmap_info_t*      store;    // NULL if disabled
  csprof_mmap_alloc_info_t allocinf; // alloc info only on active mmap
  offset_t                 sz_next;  // size of next mmap (bytes)

  // A mmap and and alloc info for temporary storage
  csprof_mmap_info_t*      store_tmp;
  csprof_mmap_alloc_info_t allocinf_tmp;
  offset_t                 sz_next_tmp;

  csprof_mem_status_t      status;

} csprof_mem_t;

//***************************************************************************
//
// Public interface to private memory allocation.
//
// Note that the available dynamic memory is strictly tied to available swap
// space.
// 
//***************************************************************************

// csprof_malloc_init: Prepares the private memory management system
// for use.  This function may be called multiple times; on each
// successive call, the memory mangement system is reset and all
// contents are lost.  If desired, users may choose initial sizes (in
// bytes) for the regular ('sz') and temporary ('sz_tmp') memory
// stores; set these parameters to 1 to use default sizes.  Either of
// the memory stores may be disabled by passing a size of 0,
// respectively, but it is an error to disable both.  Returns
// CSPROF_OK upon success; CSPROF_ERR on error.
//
// * Must be called before any allocations are performed! *
csprof_mem_t *csprof_malloc_init(offset_t sz, offset_t sz_tmp);

// csprof_malloc_fini: Cleanup and deallocate memory stores.  Returns
// CSPROF_OK upon success; CSPROF_ERR on error.
int csprof_malloc_fini(csprof_mem_t *);

// csprof_malloc: Returns a pointer to a block of memory of the
// *exact* size (in bytes) requested.  If there is insufficient
// memory, an attempt will be made to allocate more.  If this is not
// possible, an error is printed and the program is terminated.
// 
// * This memory cannot be freed! *
void* csprof_malloc(size_t size);
void* csprof_malloc_threaded(csprof_mem_t *, size_t size);

// csprof_tmalloc: Returns a pointer to a block of temporary memory of
// the *exact* size (in bytes) requested.  If there is insufficient
// memory, *no attempt* will be made to allocate more and NULL is
// returned. Temporary memory is allocated analagous to a stack; it
// may be freed using 'csprof_tfree' but it *must be* freed in the
// reverse order of allocation.
//
// csprof_tfree: Frees a block of temporary memory allocated with
// 'csprof_tmalloc'.
//
// * Be very careful when using. *
void* csprof_tmalloc(size_t size);
void* csprof_tmalloc_threaded(csprof_mem_t *, size_t size);
void csprof_tfree(void *, size_t);
void  csprof_tfree_threaded(csprof_mem_t *, void* mem, size_t size);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif
