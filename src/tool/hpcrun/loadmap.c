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
// Copyright ((c)) 2002-2022, Rice University
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

#include <libgen.h>
#include <sys/time.h>

#include "cct.h"
#include "loadmap.h"
#include "fnbounds_interface.h"
#include "fnbounds_file_header.h"
#include "hpcrun_stats.h"
#include "sample_event.h"
#include "epoch.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/spinlock.h>

#define LOADMAP_DEBUG 0

#define UW_RECIPE_MAP_DEBUG 0

static hpcrun_loadmap_t  s_loadmap;
static hpcrun_loadmap_t* s_loadmap_ptr = NULL;

static dso_info_t* s_dso_free_list = NULL;


/* locking functions to ensure that loadmaps are consistent */
static spinlock_t loadmap_lock = SPINLOCK_UNLOCKED;

static loadmap_notify_t *notification_recipients = NULL;

static void hpcrun_loadModule_flags_init(load_module_t *lm);

void
hpcrun_loadmap_notify_register(loadmap_notify_t *n)
{
  n->next = notification_recipients;
  notification_recipients = n;
}


static void
hpcrun_loadmap_notify_map(load_module_t* lm)
{
  loadmap_notify_t * n = notification_recipients;
  while (n) { 
    if (n->map) {
      n->map(lm);
    }
    n = n->next;
  }
}


static void
hpcrun_loadmap_notify_unmap(load_module_t* lm)
{
  loadmap_notify_t * n = notification_recipients;
  while (n) {
    if (n->unmap) {
      n->unmap(lm);
    }
    n = n->next;
  }
}


//***************************************************************************
// 
//***************************************************************************

dso_info_t*
hpcrun_dso_new()
{
  dso_info_t* x = NULL;

  if (s_dso_free_list) {
    x = s_dso_free_list;
    s_dso_free_list = s_dso_free_list->next;
    if (s_dso_free_list) {
      s_dso_free_list->prev = NULL;
    }
    x->next = NULL; // prev should already be NULL
  }
  else {
    TMSG(DSO, " hpcrun_dso_new");
    x = (dso_info_t*) hpcrun_malloc(sizeof(dso_info_t));
  }

  return x;
}


dso_info_t*
hpcrun_dso_make(const char* name, void** table,
		struct fnbounds_file_header* fh,
		void* startaddr, void* endaddr, unsigned long map_size)
{
  dso_info_t* x = hpcrun_dso_new();
  
  TMSG(DSO," hpcrun_dso_make for module %s", name);

  int namelen = strlen(name) + 1;
  x->name = (char*) malloc(namelen);
  strcpy(x->name, name);

  x->table = table;
  x->map_size = map_size;
  x->nsymbols = 0;
  x->start_to_ref_dist = 0;
  x->start_addr = startaddr;
  x->end_addr = endaddr;

  if (fh) {
    x->nsymbols = (unsigned long)fh->num_entries;
    x->is_relocatable = fh->is_relocatable;

    // Cf. hpcrun_normalize_ip(): Given ip, compute lm_ip:
    //   lm_ip = (ip - lm_mapped_start) + lm_ip_ref
    //         = ip - (lm_mapped_start - lm_ip_ref)
    //         = ip - start_to_ref_dist
    if (fh->is_relocatable) {
      x->start_to_ref_dist = (uintptr_t)startaddr - fh->reference_offset;
    }
  }
  x->next = NULL;
  x->prev = NULL;

  TMSG(DSO, "new dso: start = %p, end = %p, name = %s",
       startaddr, endaddr, name);

  return x;
}


//***************************************************************************

void
hpcrun_dsoList_dump(dso_info_t* dl_list)
{
  for (dso_info_t* x = dl_list; (x); x = x->next) {
    hpcrun_dso_dump(x);
  }
}


void
hpcrun_dso_dump(dso_info_t* x)
{
  printf("%p-%p %s [dso_info_t *%p, table=%p, nsymbols=%ld, relocatable=%d]\n",
	 x->start_addr, x->end_addr, x->name, 
         x, x->table, x->nsymbols, x->is_relocatable);
}


//***************************************************************************
// 
//***************************************************************************

load_module_t*
hpcrun_loadModule_new(const char* name)
{
  load_module_t* x = (load_module_t*) hpcrun_malloc(sizeof(load_module_t));

  //memset(x, 0, sizeof(*x));

  x->id = ++(s_loadmap_ptr->size); // largest id = size

  int namelen = strlen(name) + 1;
  x->name = (char*) hpcrun_malloc(namelen);
  strcpy(x->name, name);

  x->dso_info = NULL;
  x->next = NULL;
  x->prev = NULL;
  x->phdr_info.dlpi_phdr = NULL;

  hpcrun_loadModule_flags_init(x);

  return x;
}



//***************************************************************************
// 
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
  hpcrun_loadmap_t* x = hpcrun_malloc(sizeof(hpcrun_loadmap_t));
  if (x == NULL) {
    EMSG("New loadmap requested, but allocation failed!!");
    return NULL;
  }

  hpcrun_loadmap_init(x);
  
  return x;
}


void
hpcrun_loadmap_init(hpcrun_loadmap_t* x)
{
  TMSG(LOADMAP, "init");
  memset(x, 0, sizeof(*x));
  
  x->lm_head = NULL;
  x->lm_end = NULL;
  x->size = 0;
}

//
// debugging operation, print loadmap in reverse order
//
void
hpcrun_loadmap_print(hpcrun_loadmap_t* loadmap)
{
  for (load_module_t* lm = loadmap->lm_end; lm; lm = lm->prev) {
    printf("%u  %s\n", lm->id, lm->name);
  }
}

//***************************************************************************

load_module_t*
hpcrun_loadmap_findByAddr(void* begin, void* end)
{
  // don't waste effort on an obviously invalid address
  if (begin == 0) return NULL; 

  TMSG(LOADMAP, "find by address %p -- %p", begin, end);

  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    TMSG(LOADMAP, "\tload module %s", x->name);
    if (x->dso_info) {
      TMSG(LOADMAP, "\t\t [%lx, %lx) table [%lx, %lx)", 
	   (uintptr_t) x->dso_info->start_addr,
	   (uintptr_t) x->dso_info->end_addr,
	   ((uintptr_t) x->dso_info->table ? ((uintptr_t) x->dso_info->table[0] + 
				  x->dso_info->start_to_ref_dist) : -1),
	   ((uintptr_t) x->dso_info->table ? ((uintptr_t) x->dso_info->table[x->dso_info->nsymbols -1] +
				  x->dso_info->start_to_ref_dist) : -1)
	   );
      if (x->dso_info->start_addr <= begin && end <= x->dso_info->end_addr) {
        TMSG(LOADMAP, "       --->%s", x->name);
        hpcrun_loadModule_flags_set(x, LOADMAP_ENTRY_ANALYZE);
        return x;
      }
    }
  }
  TMSG(LOADMAP, "       --->(NOT FOUND)");
  return NULL;
}


load_module_t*
hpcrun_loadmap_findByName(const char* name)
{
  TMSG(LOADMAP, "find by name: %s", name);
  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    if (strcmp(x->name, name) == 0) {
      TMSG(LOADMAP, "       --->FOUND", x->name);
      return x;
    }
  }
  TMSG(LOADMAP, "       --->(NOT FOUND)");
  return NULL;
}

load_module_t*
hpcrun_loadmap_findById(uint16_t id)
{
  TMSG(LOADMAP, "find by id %d", id);
  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    if (x->id == id) {
      TMSG(LOADMAP, "       --->%s", x->name);
      return x;
    }
  }
  TMSG(LOADMAP, "       --->(NOT FOUND)");
  return NULL;
}

const char*
hpcrun_loadmap_findLoadName(const char* name)
{
  TMSG(LOADMAP, "find load name: %s", name);
  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    char *bn = basename(x->name);
    if (bn && strcmp(bn, name) == 0) {
      TMSG(LOADMAP, "       --->%s", x->name);
      return x->name;
    }
  }
  TMSG(LOADMAP, "       --->(NOT FOUND)");
  return NULL;
}


//***************************************************************************

static void
hpcrun_loadmap_pushFront(load_module_t* lm)
{
  TMSG(LOADMAP, "push front: %s", lm->name);
  // link 'm' at the head of the list of loaded modules
  if (s_loadmap_ptr->lm_head) {
    TMSG(LOADMAP, "previous front = %s", s_loadmap_ptr->lm_head->name);
    s_loadmap_ptr->lm_head->prev = lm;
    lm->next = s_loadmap_ptr->lm_head;
    lm->prev = NULL;
    s_loadmap_ptr->lm_head = lm;
  }
  else {
    TMSG(LOADMAP, " ->First entry");
    s_loadmap_ptr->lm_head = lm;
    s_loadmap_ptr->lm_end = lm;
    lm->next = NULL;
    lm->prev = NULL;
  }
}


#if 0
// Pushes 'lm' to the end of the current loadmap. Should only occur
// when lm's dso_info field has become invalidated, thus creating a
// sub-list of invalid load modules at the end of the loadmap.
static void
hpcrun_loadmap_moveToBack(load_module_t* lm)
{
  // short-circuit if lm is already at end of list
  if (lm == s_loadmap_ptr->lm_end) {
    return;
  }

  // -------------------------------------------------------
  // INVARIANT: lm is not at the end of the list
  // -------------------------------------------------------

  if (lm->prev) {
    lm->prev->next = lm->next;
  }
  else { // if lm->prev == NULL => lm == s_loadmap_ptr->lm_head
    s_loadmap_ptr->lm_head = lm->next;
  }
  
  if (lm->next) {
    lm->next->prev = lm->prev;
  }
  
  lm->prev = s_loadmap_ptr->lm_end;
  lm->prev->next = lm;
  lm->next = NULL;
  s_loadmap_ptr->lm_end = lm;
}
#endif


static void
hpcrun_loadmap_dump_dl_phdr_info(struct dl_phdr_info *x)
{
  fprintf(stderr, "\t phdr_info %p\n", x);
  if (x) {
    fprintf(stderr, "\t\t dlpi_addr 0x%lx\n", x->dlpi_addr);
    fprintf(stderr, "\t\t dlpi_name %s\n", x->dlpi_name);
    fprintf(stderr, "\t\t dlpi_phnum %d\n", x->dlpi_phnum);
    fprintf(stderr, "\t\t dlpi_adds %llu\n", x->dlpi_adds);
    fprintf(stderr, "\t\t dlpi_subs %llu\n", x->dlpi_adds);
    fprintf(stderr, "\t\t dlpi_tls_modid %ld\n", x->dlpi_tls_modid);
    fprintf(stderr, "\t\t dlpi_tls_data %p\n", x->dlpi_tls_data);
  }
}


static void
hpcrun_loadmap_dump_dso_info_t(dso_info_t* x)
{
  fprintf(stderr, "\t dso_info_t %p\n", x);
  if (x) {
    fprintf(stderr, "\t\t name %s\n", x->name);
    fprintf(stderr, "\t\t start_addr %p\n", x->start_addr);
    fprintf(stderr, "\t\t end_addr %p\n", x->end_addr);
    fprintf(stderr, "\t\t map_size %lu\n", x->map_size);
    fprintf(stderr, "\t\t nsymbols %lu\n", x->nsymbols);
    fprintf(stderr, "\t\t is_relocatable %d\n", x->is_relocatable);
  }
}


static void
hpcrun_loadmap_dump_load_module_t(load_module_t* x)
{
  fprintf(stderr, "load_module_t %p\n", x);
  fprintf(stderr, "\t id %d\n", x->id);
  fprintf(stderr, "\t name %s\n", x->name);
  hpcrun_loadmap_dump_dso_info_t(x->dso_info);
  hpcrun_loadmap_dump_dl_phdr_info(&(x->phdr_info));
  fprintf(stderr, "\t next %p\n", x->next);
  fprintf(stderr, "\t prev %p\n", x->prev);
}


load_module_t*
hpcrun_loadmap_map(dso_info_t* dso)
{
  const char* msg = "";

  TMSG(LOADMAP, "map in dso %s", dso->name);
  // -------------------------------------------------------
  // Find or create a load_module_t: if a load module exists
  // with same name, reuse it; otherwise create a new entry
  // -------------------------------------------------------
  load_module_t* lm = hpcrun_loadmap_findByName(dso->name);
  if (lm) {
    // sanity check to ensure internal consistency
    if (lm->dso_info != dso) {
      TMSG(LOADMAP, " !! Internal consistency check fires !!");
      hpcrun_loadmap_unmap(lm);
      lm->dso_info = dso;
    }
    else {
      EMSG("hpcrun_loadmap_map(): attempt to both map dso '%s' and place it on the free list!", dso->name);
    }
    msg = "(reuse)";
  }
  else {
	lm = hpcrun_loadModule_new(dso->name);
	lm->dso_info = dso;
	hpcrun_loadmap_pushFront(lm);

#if UW_RECIPE_MAP_DEBUG
        fprintf(stderr, "hpcrun_loadmap_map: '%s' start=%p end=%p\n", 
                dso->name, lm->dso_info->start_addr, lm->dso_info->end_addr);
#endif

  }

  hpcrun_loadmap_notify_map(lm);

  TMSG(LOADMAP, "hpcrun_loadmap_map: '%s' size=%d %s",
       dso->name, s_loadmap_ptr->size, msg);

  return lm;
}


void
hpcrun_loadmap_unmap(load_module_t* lm)
{
  TMSG(LOADMAP,"hpcrun_loadmap_unmap: '%s'", lm->name);

  dso_info_t* old_dso = lm->dso_info;

  if (old_dso == NULL) return; // nothing to do!  

  // need to notify unmap clients before setting dso_info to NULL
  hpcrun_loadmap_notify_unmap(lm);

  lm->dso_info = NULL;

  // Set dl_phdr_info structure to uninitialized state
  lm->phdr_info.dlpi_phdr = NULL;

  // tallent: For now, do not move the loadmap to the back of the
  //   list.  If we want to enable, this, we could have
  //   hpcrun_loadmap_findByName() begin its search from the end
  //   of the list.
  //hpcrun_loadmap_moveToBack(lm);

  // add old_dso to the head of the s_dso_free_list
  old_dso->next = s_dso_free_list;
  old_dso->prev = NULL;
  if (s_dso_free_list) {
    s_dso_free_list->prev = old_dso;
  }
  s_dso_free_list = old_dso;
  TMSG(LOADMAP, "Deleting unw intervals");

#if LOADMAP_DEBUG
  assert((uintptr_t)(old_dso->end_addr) < UINTPTR_MAX) ;
#endif

#if UW_RECIPE_MAP_DEBUG
  fprintf(stderr, "hpcrun_loadmap_unmap: '%s' start=%p end=%p\n", 
          lm->name, old_dso->start_addr, old_dso->end_addr);
#endif
}


// used only to add a load module for the kernel 
uint16_t 
hpcrun_loadModule_add(const char* name)
{
  load_module_t *lm = hpcrun_loadModule_new(name);
  hpcrun_loadmap_pushFront(lm);
  return lm->id;
}


static void
hpcrun_loadModule_flags_init(load_module_t *lm)
{
  atomic_store(&(lm->flags), 0);
}


void
hpcrun_loadModule_flags_set(load_module_t *lm, int flag)
{
  atomic_fetch_or(&(lm->flags), flag);
}


int
hpcrun_loadModule_flags_get(load_module_t *lm)
{
  return atomic_load(&(lm->flags));
}


//***************************************************************************
// 
//***************************************************************************

void
hpcrun_initLoadmap()
{
  // all processes (initial and forked processes)
  notification_recipients = NULL; 

  // initial process only
  if (s_loadmap_ptr == NULL) { 

    // initialize load map itself
    s_loadmap_ptr = &s_loadmap;
    hpcrun_loadmap_init(s_loadmap_ptr);

    // initialize free list for shared libraries
    s_dso_free_list = NULL;
  }
}


hpcrun_loadmap_t*
hpcrun_getLoadmap()
{
  return s_loadmap_ptr;
}


int
hpcrun_loadmap_iterate
(
  int (*cb)(struct dl_phdr_info * info, size_t size, void* data),
  void *data
)
{  
  int ret = 0;
  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    // Skip load modules that do not have a valid dl_phdr_info structure
    if (x->phdr_info.dlpi_phdr == NULL) continue;
    ret = cb(&(x->phdr_info), sizeof(struct dl_phdr_info), data);
    if (ret != 0) return ret;    
  }  
  return ret;
}


void
hpcrun_loadmap_dump()
{
  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    hpcrun_loadmap_dump_load_module_t(x);
  }
}
