// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/loadmap.c $
// $Id: loadmap.c 2799 2010-04-01 19:14:19Z mfagan $
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

/*
  Copyright ((c)) 2002, Rice University 
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

hpcrun_loadmap_t*
hpcrun_static_loadmap() 
{
  return &static_loadmap;
}

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
hpcrun_loadmap_is_locked()
{
  return spinlock_is_locked(&loadmap_lock);
}

void
hpcrun_loadmap_add_module(dso_info_t* dso)
{
  TMSG(LOADMAP," loadmap_add_module('%s')", dso->name);

  load_module_t *m = hpcrun_find_lm_by_name(dso->name);
  if (m) {
    hpcrun_remove_dso(m); 
    m->lm_info = dso;
  }
  else {
    m =(load_module_t *) hpcrun_malloc(sizeof(load_module_t)); 
    //never delete the load modules
    
    int namelen = strlen(dso->name) + 1;
    // fill in the fields of the structure
    m->id = ++current_loadmap->size;
    m->name = (char *) hpcrun_malloc(namelen);
    strcpy(m->name, dso->name);
    m->lm_info = dso;
    
    TMSG(LOADMAP," loadmap_add_module: new size=%d", 
	 current_loadmap->size);
  }
  // link it at the head of the list of loaded modules for the current loadmap
  if (current_loadmap->lm_head) {
    current_loadmap->lm_head->prev= m;
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
  
  /* make sure everything is up-to-date before setting the
     current_loadmap */
  current_loadmap = e;
}

hpcrun_loadmap_t*
hpcrun_loadmap_new(void)
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

//only found if has a valid reference to a dso_info_t
load_module_t*
hpcrun_find_lm_by_addr(void* begin, void* end)
{
  load_module_t* lm_src = current_loadmap->lm_head;
  
  while (lm_src && lm_src->lm_info && (begin < lm_src->lm_info->start_addr 
				       || end > lm_src->lm_info->end_addr)) {
    lm_src = lm_src->next;
  }
  
  if (lm_src && lm_src->lm_info) {
    return lm_src;
  }
  
  return NULL;
}

//finds regardless of whether or not lm_src has a non-NULL lm_info field.
load_module_t*
hpcrun_find_lm_by_name(char* name)
{
  load_module_t* lm_src = current_loadmap->lm_head;

  while (lm_src && strcmp(lm_src->name, name) != 0 ) {
    lm_src = lm_src->next;
  }

  return lm_src;
}
  
void 
hpcrun_move_to_back(load_module_t* lm)
{
  if (lm->prev) {
    lm->prev->next = lm->next;
  } else { //if lm->prev == NULL => lm == current_loadmap->lm_head
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

dso_info_t*
hpcrun_free_list()
{
  return dso_free_list;
}


dso_info_t *
new_dso_info_t(const char *name, void **table, struct fnbounds_file_header *fh,
	       void *startaddr, void *endaddr, long map_size)
{
  int namelen = strlen(name) + 1;
  dso_info_t *r  = hpcrun_dso_allocate();
  
  TMSG(DSO," new_dso_info_t for module %s", name);
  r->name = (char *) hpcrun_malloc(namelen);
  strcpy(r->name, name);
  r->table = table;
  r->map_size = map_size;
  r->nsymbols = fh->num_entries;
  r->relocate = fh->relocatable;
  r->start_addr = startaddr;
  r->end_addr = endaddr;
  r->start_to_ref_dist = (unsigned long) startaddr - fh->reference_offset;
  r->next = NULL;
  r->prev = NULL;
  // NOTE: offset = relocation offset

  hpcrun_loadmap_add_module(r); //add r to the loadmap
  TMSG(DSO, "new dso: start = %p, end = %p, name = %s",
       startaddr, endaddr, name);

  return r;
}


//*********************************************************************
// deallocation/Allocation of dso_info_t records
//*********************************************************************

dso_info_t *
hpcrun_dso_allocate()
{
  dso_info_t* new;
  if (dso_free_list) {
    new = dso_free_list; //new = head of dso_free_list
    dso_free_list = dso_free_list->next;
    if (dso_free_list) {
      dso_free_list->prev = NULL;
    }
    new->next = NULL; //prev should already be NULL
  } else {
    TMSG(DSO," dso_info_allocate");
    new = (dso_info_t *) hpcrun_malloc(sizeof(dso_info_t));
  }
  return new;
}

void
hpcrun_remove_dso(load_module_t *module)
{
  TMSG(LOADMAP,"load module %s: removing dso", module->name);

  dso_info_t* unused = module->lm_info;
  module->lm_info = NULL;
  hpcrun_move_to_back(module); //by pushisng lm's with removed dso's
                               //to the back don't have to search
                               //entire list.
  if (unused) {
    //add unused to the head of the dso_free_list
    unused->next = dso_free_list;
    unused->prev = NULL;
    if (dso_free_list) {
      dso_free_list->prev = unused;
    }
    dso_free_list = unused;
  }
  TMSG(LOADMAP,"load module %s: removed dso", module->name);
}


//*********************************************************************
// debugging support
//*********************************************************************

void 
dump_dso_info_t(dso_info_t *r)
{
  printf("%p-%p %s [dso_info_t *%p, table=%p, nsymbols=%d, relocatable=%d]\n",
	 r->start_addr, r->end_addr, r->name, 
         r, r->table, r->nsymbols, r->relocate);
}


void 
dump_dso_list(dso_info_t *dl_list)
{
  dso_info_t *r = dl_list;
  for (; r; r = r->next) dump_dso_info_t(r);
}


hpcrun_loadmap_t*
hpcrun_get_loadmap()
{
  return current_loadmap;
}
