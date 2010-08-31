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

#include <sys/time.h>
#include "cct.h"
#include "loadmap.h"
#include "fnbounds_interface.h"
#include "hpcrun_stats.h"
#include "sample_event.h"
#include "epoch.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/spinlock.h>


static hpcrun_loadmap_t static_loadmap;
static hpcrun_loadmap_t *current_loadmap = NULL;
static dso_info_t *dso_free_list = NULL;


/* locking functions to ensure that loadmaps are consistent */
static spinlock_t loadmap_lock = SPINLOCK_UNLOCKED;


//***************************************************************************

hpcrun_loadmap_t*
hpcrun_static_loadmap() 
{
  return &static_loadmap;
}


hpcrun_loadmap_t*
hpcrun_get_loadmap()
{
  return current_loadmap;
}


//***************************************************************************

void
hpcrun_loadmap_lock() 
{
  spinlock_lock(&loadmap_lock);
}


void
hpcrun_loadmap_unlock()
{
  spinlock_unlock(&loadmap_lock);
}


int
hpcrun_loadmap_isLocked()
{
  return spinlock_is_locked(&loadmap_lock);
}


//***************************************************************************

hpcrun_loadmap_t*
hpcrun_loadmap_new()
{
  TMSG(LOADMAP, " --NEW");
  hpcrun_loadmap_t* e = hpcrun_malloc(sizeof(hpcrun_loadmap_t));
  if (e == NULL) {
    EMSG("New loadmap requested, but allocation failed!!");
    return NULL;
  }

  hpcrun_loadmap_init(e);
  
  return e;
}


void
hpcrun_loadmap_init(hpcrun_loadmap_t* e)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  TMSG(LOADMAP, "new loadmap created");
  TMSG(LOADMAP, "new loadmap time: sec = %ld, usec = %d, samples = %ld",
       (long)tv.tv_sec, (int)tv.tv_usec, hpcrun_stats_num_samples_total());
  
  memset(e, 0, sizeof(*e));
  
  e->lm_head = NULL;
  e->lm_end = NULL;
  e->size = 0;
  
  current_loadmap = e;
}


//***************************************************************************

load_module_t*
hpcrun_loadmap_findByAddr(void* begin, void* end)
{
  for (load_module_t* x = current_loadmap->lm_head; (x); x = x->next) {
    if (x->dso_info
	&& x->dso_info->start_addr <= begin && end <= x->dso_info->end_addr) {
      return x;
    }
  }

  return NULL;
}


load_module_t*
hpcrun_loadmap_findByName(char* name)
{
  for (load_module_t* x = current_loadmap->lm_head; (x); x = x->next) {
    if (strcmp(x->name, name) == 0) {
      return x;
    }
  }

  return NULL;
}


//***************************************************************************

void
hpcrun_loadmap_addModule(dso_info_t* dso)
{
  TMSG(LOADMAP," loadmap_add_module('%s')", dso->name);

  // -------------------------------------------------------
  // Find or create a load_module_t: if a load_module_t with same name
  // exists, reuse that; otherwise create a new load_module_t
  // -------------------------------------------------------
  load_module_t* m = hpcrun_loadmap_findByName(dso->name);
  if (m) {
    hpcrun_load_module_remove_dso(m);
    m->dso_info = dso;
  }
  else {
    m = (load_module_t*) hpcrun_malloc(sizeof(load_module_t)); 
    
    int namelen = strlen(dso->name) + 1;

    m->id = ++current_loadmap->size; // largest id = size
    m->name = (char *) hpcrun_malloc(namelen);
    strcpy(m->name, dso->name);
    m->dso_info = dso;
    
    TMSG(LOADMAP, " loadmap_add_module: new size=%d", current_loadmap->size);

    // link 'm' at the head of the list of loaded modules
    if (current_loadmap->lm_head) {
      current_loadmap->lm_head->prev = m;
      m->next = current_loadmap->lm_head;
      m->prev = NULL;
      current_loadmap->lm_head = m;
    }
    else {
      current_loadmap->lm_head = m;
      current_loadmap->lm_end = m;
      m->next = NULL;
      m->prev = NULL;
    }
  }
}


//***************************************************************************
// deallocation/allocation of dso_info_t records
//***************************************************************************

dso_info_t*
hpcrun_dso_new()
{
  dso_info_t* new;
  if (dso_free_list) {
    new = dso_free_list; //new = head of dso_free_list
    dso_free_list = dso_free_list->next;
    if (dso_free_list) {
      dso_free_list->prev = NULL;
    }
    new->next = NULL; //prev should already be NULL
  }
  else {
    TMSG(DSO, " hpcrun_dso_new");
    new = (dso_info_t*) hpcrun_malloc(sizeof(dso_info_t));
  }
  return new;
}


dso_info_t*
hpcrun_dso_make(const char* name, void** table,
		struct fnbounds_file_header* fh,
		void* startaddr, void* endaddr, long map_size)
{
  dso_info_t* x = hpcrun_dso_new();
  
  TMSG(DSO," hpcrun_dso_make for module %s", name);
  int namelen = strlen(name) + 1;
  x->name = (char *) hpcrun_malloc(namelen);
  strcpy(x->name, name);
  x->table = table;
  x->map_size = map_size;
  x->nsymbols = fh->num_entries;
  x->relocate = fh->relocatable;
  x->start_addr = startaddr;
  x->end_addr = endaddr;
  x->start_to_ref_dist = (unsigned long) startaddr - fh->reference_offset;
  x->next = NULL;
  x->prev = NULL;
  // NOTE: start_to_ref_dist = pointer to reference address

  TMSG(DSO, "new dso: start = %p, end = %p, name = %s",
       startaddr, endaddr, name);

  return x;
}


void
hpcrun_load_module_remove_dso(load_module_t* module)
{
  TMSG(LOADMAP,"load module %s: removing dso", module->name);

  dso_info_t* unused = module->dso_info;
  module->dso_info = NULL;
  hpcrun_move_to_back(module);
  
  if (unused) {
    // add unused to the head of the dso_free_list
    unused->next = dso_free_list;
    unused->prev = NULL;
    if (dso_free_list) {
      dso_free_list->prev = unused;
    }
    dso_free_list = unused;
  }
  TMSG(LOADMAP,"load module %s: removed dso", module->name);
}


void 
hpcrun_move_to_back(load_module_t* lm)
{
  if (lm->prev) {
    lm->prev->next = lm->next;
  }
  else { //if lm->prev == NULL => lm == current_loadmap->lm_head
    current_loadmap->lm_head = lm->next;
  }

  if (lm->next) {
    lm->next->prev = lm->prev;
  }

  lm->prev = current_loadmap->lm_end;
  lm->prev->next = lm;
  lm->next = NULL;
  current_loadmap->lm_end = lm;
}


//***************************************************************************
// debugging support
//***************************************************************************

void 
dump_dso_list(dso_info_t* dl_list)
{
  for (dso_info_t* x = dl_list; (x); x = x->next) {
    dump_dso_info_t(x);
  }
}


void 
dump_dso_info_t(dso_info_t* x)
{
  printf("%p-%p %s [dso_info_t *%p, table=%p, nsymbols=%d, relocatable=%d]\n",
	 x->start_addr, x->end_addr, x->name, 
         x, x->table, x->nsymbols, x->relocate);
}


