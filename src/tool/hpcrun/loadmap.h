// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <tool/hpcfnbounds/fnbounds-file-header.h>


//***************************************************************************
//
//***************************************************************************

typedef struct dso_info_s {
  char* name;
  void* start_addr;
  void* end_addr;
  uintptr_t start_to_ref_dist;
  void** table;
  long map_size;
  int  nsymbols;
  int  relocate;

  struct dso_info_s* next; //to only be used with dso_free_list
  struct dso_info_s* prev;
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
		void* startaddr, void* endaddr, long map_size);


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
  struct load_module_t* next;
  struct load_module_t* prev;
} load_module_t;


typedef struct hpcrun_loadmap_t
{
  uint16_t size; //can also double as nextId (size + 1)
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
void
hpcrun_loadmap_init(hpcrun_loadmap_t* x);


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

// Find the (currently mapped) load module that 'contains' the address
//   range [begin, end]
load_module_t*
hpcrun_loadmap_findByAddr(void* begin, void* end);


// Find a load module by name.
load_module_t*
hpcrun_loadmap_findByName(char* name);


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

// Adds a new load module to the current load map. 'dso' is assumed to
// be non-NULL. If a module with the same name as 'dso' is found, then
// 'dso' is inserted into that load module, otherwise, a new load
// module is created. Always places the modified load module to the
// front of the load map
void
hpcrun_loadmap_map(dso_info_t* dso);


// Removes a dso_info_t struct by placing it into the free list.
void
hpcrun_loadmap_unmap(load_module_t* module);


//***************************************************************************
//
//***************************************************************************

void
hpcrun_initLoadmap();

hpcrun_loadmap_t*
hpcrun_getLoadmap();


//***************************************************************************

#endif // LOADMAP_H
