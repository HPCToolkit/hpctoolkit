// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef LOADMAP_H
#define LOADMAP_H

#include <stdio.h>

/* an "loadmap" is an interval of time during which no two dynamic
   libraries are mapped to the same region of the address space.
   an loadmap can span across dlopen and dlclose operations. an loadmap
   ends when a dlopen maps a new load module on top of a region of
   the address space that has previously been occupied by another
   module earlier during the loadmap.
*/

// Local includes

#include "../common/lean/hpcio.h"
#include "../common/lean/hpcfmt.h"
#include "../common/lean/hpcrun-fmt.h"

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

#include "fnbounds/fnbounds_file_header.h"

#include <link.h>

//***************************************************************************
// macros
//***************************************************************************

#define LOADMAP_INVALID_MODULE_ID (~0)

//***************************************************************************
// types
//***************************************************************************

typedef struct dso_info_t {
  char* name;
  void* start_addr;
  void* end_addr;
  uintptr_t start_to_ref_dist;
  void** table;
  unsigned long nsymbols;
  int  is_relocatable;

  struct dso_info_t* next; //to only be used with dso_free_list
  struct dso_info_t* prev;

} dso_info_t;


// ---------------------------------------------------------
//
// ---------------------------------------------------------

// Constructs a new dso_info_t by either pulling an unused one from
// the free list or malloc-ing one.  If there are any on the free
// list, will return a pointer to it, otherwise will malloc a new one.
dso_info_t*
hpcrun_dso_new();


// Allocates and initializes a dso_info_t
dso_info_t*
hpcrun_dso_make(const char* name, void** table,
                struct fnbounds_file_header* fh,
                void* startaddr, void* endaddr);


// ---------------------------------------------------------
//
// ---------------------------------------------------------

// Use to dump the free list
void
hpcrun_dsoList_dump(dso_info_t* dl_list);


// Use to dump a single dso_info_t struct.
void
hpcrun_dso_dump(dso_info_t* x);


//***************************************************************************
//
//***************************************************************************

typedef struct load_module_t
{
  uint16_t id;
  char* name;
  dso_info_t* dso_info;
  struct dl_phdr_info phdr_info;
  struct load_module_t* next;
  struct load_module_t* prev;
#ifdef __cplusplus
  std::
#endif
  atomic_int flags;
} load_module_t;


load_module_t*
hpcrun_loadModule_new(const char* name);

// used only to add a load module for the kernel
uint16_t
hpcrun_loadModule_add(const char* name);

void
hpcrun_loadModule_flags_set(load_module_t *lm, int flag);

int
hpcrun_loadModule_flags_get(load_module_t *lm);


//***************************************************************************
//
//***************************************************************************

typedef struct hpcrun_loadmap_t
{
  uint16_t size; // implies the next load_module_t id (size + 1)
  load_module_t* lm_head;
  load_module_t* lm_end;

} hpcrun_loadmap_t;


// ---------------------------------------------------------
//
// ---------------------------------------------------------
void
hpcrun_loadmap_lock();


void
hpcrun_loadmap_unlock();


int
hpcrun_loadmap_isLocked();


// ---------------------------------------------------------
//
// ---------------------------------------------------------

// Requests a new load map.
hpcrun_loadmap_t*
hpcrun_loadmap_new();


// Initializes the load map
extern void hpcrun_loadmap_init(hpcrun_loadmap_t* x);

//
// debugging operation, print loadmap in reverse order
//
extern void hpcrun_loadmap_print(hpcrun_loadmap_t* loadmap);

// ---------------------------------------------------------
//
// ---------------------------------------------------------

// hpcrun_loadmap_findByAddr: Find the (currently mapped) load module
//   that 'contains' the address range [begin, end]
load_module_t*
hpcrun_loadmap_findByAddr(void* begin, void* end);


// hpcrun_loadmap_findByName: Find a load module by name.
load_module_t*
hpcrun_loadmap_findByName(const char* name);

// find a load module, given the (previously determined) id
load_module_t*
hpcrun_loadmap_findById(uint16_t id);


// hpcrun_loadmap_findLoadName: Search loadmap for (full) name of
//   entry that has "name" as its executable name
const char*
hpcrun_loadmap_findLoadName(const char* name);


// ---------------------------------------------------------
//
// ---------------------------------------------------------

// hpcrun_loadmap_map: Add a load module based on 'dso' to the current
//   load map, ensuring that dso's name appears exactly once in the
//   load map. 'dso' is assumed to be non-NULL.  Locates the new load
//   module at the front of the load map.
load_module_t*
hpcrun_loadmap_map(dso_info_t* dso);


// hpcrun_loadmap_unmap: Note that 'lm' has been unmapped but retain a
//   reference to it within the load map.
void
hpcrun_loadmap_unmap(load_module_t* lm);


//***************************************************************************
//
//***************************************************************************

void
hpcrun_initLoadmap();

hpcrun_loadmap_t*
hpcrun_getLoadmap();

typedef void (*loadmap_notify_range_t)(load_module_t*);

typedef struct loadmap_notify_t {
  loadmap_notify_range_t map;
  loadmap_notify_range_t unmap;
  struct loadmap_notify_t *next;
} loadmap_notify_t;

void hpcrun_loadmap_notify_register(loadmap_notify_t *n);

int
hpcrun_loadmap_iterate
(
  int (*cb)(struct dl_phdr_info * info, size_t size, void* data),
  void *data
);


//***************************************************************************

#endif // LOADMAP_H
