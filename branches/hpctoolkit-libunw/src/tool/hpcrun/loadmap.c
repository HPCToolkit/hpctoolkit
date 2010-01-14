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
// Copyright ((c)) 2002-2009, Rice University 
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

hpcrun_loadmap_t*
hpcrun_get_loadmap()
{
  return current_loadmap;
}


void
hpcrun_loadmap_add_module(const char *module_name,
			  void *vaddr,                /* the preferred virtual address */
			  void *mapaddr,              /* the actual mapped address */
			  size_t size)                /* end addr minus start addr */
{
  TMSG(LOADMAP," loadmap_add_module");

  loadmap_src_t *m = (loadmap_src_t *) hpcrun_malloc(sizeof(loadmap_src_t));

  // fill in the fields of the structure
  m->id = 0; // FIXME:tallent
  m->name = (char*) module_name;
  m->vaddr = vaddr;
  m->mapaddr = mapaddr;
  m->size = size;

  // link it into the list of loaded modules for the current loadmap
  m->next = current_loadmap->loaded_modules;
  current_loadmap->loaded_modules = m; 
  current_loadmap->num_modules++;
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

  e->loaded_modules = NULL;

  /* make sure everything is up-to-date before setting the
     current_loadmap */
  e->next = current_loadmap;
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

void
hpcrun_finalize_current_loadmap(void)
{
  TMSG(LOADMAP, " --Finalize current");
  // lazily finalize the last loadmap
  hpcrun_loadmap_lock();
  if (current_loadmap->loaded_modules == NULL) {
    fnbounds_loadmap_finalize();
  }
  hpcrun_loadmap_unlock();
}
