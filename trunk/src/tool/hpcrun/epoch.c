// -*-Mode: C++;-*- // technically C99
// $Id$

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
#include "epoch.h"
#include "fnbounds_interface.h"
#include "interface.h"
#include "mem.h"
#include "pmsg.h"
#include "sample_event.h"
#include "spinlock.h"
#include "state.h"

#include <lib/prof-lean/hpcfmt.h>


static csprof_epoch_t static_epoch;

/* epochs are entirely separate from profiling state */
static csprof_epoch_t *current_epoch = NULL;

/* locking functions to ensure that epochs are consistent */
static spinlock_t epoch_lock = SPINLOCK_UNLOCKED;

csprof_epoch_t *
csprof_static_epoch() 
{
  return &static_epoch;
}

void
csprof_epoch_lock() 
{
  spinlock_lock(&epoch_lock);
}

void
csprof_epoch_unlock()
{
  spinlock_unlock(&epoch_lock);
}

int
csprof_epoch_is_locked()
{
  return spinlock_is_locked(&epoch_lock);
}

csprof_epoch_t *
csprof_get_epoch()
{
  return current_epoch;
}


void
hpcrun_loadmap_add_module(const char *module_name, 
			  void *vaddr,                /* the preferred virtual address */
			  void *mapaddr,              /* the actual mapped address */
			  size_t size)                /* end addr minus start addr */
{
  TMSG(MALLOC," epoch_add_module");

  loadmap_src_t *m = (loadmap_src_t *) csprof_malloc(sizeof(loadmap_src_t));

  // fill in the fields of the structure
  m->name = (char *) module_name;
  m->vaddr = vaddr;
  m->mapaddr = mapaddr;
  m->size = size;

  // link it into the list of loaded modules for the current epoch
  m->next = current_epoch->loaded_modules;
  current_epoch->loaded_modules = m; 
  current_epoch->num_modules++;
}



/* epochs are totally distinct from profiling states */
void
csprof_epoch_init(csprof_epoch_t *e)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  TMSG(EPOCH, "new epoch created");
  TMSG(EPOCH, "new epoch time: sec = %ld, usec = %d, samples = %ld",
       (long)tv.tv_sec, (int)tv.tv_usec, csprof_num_samples_total());

  memset(e, 0, sizeof(*e));

  e->loaded_modules = NULL;

  /* make sure everything is up-to-date before setting the
     current_epoch */
  e->next = current_epoch;
  current_epoch = e;
}

/* epochs are totally distinct from profiling states */
csprof_epoch_t*
csprof_epoch_new()
{
  TMSG(MALLOC," epoch-new");
  csprof_epoch_t *e = csprof_malloc(sizeof(csprof_epoch_t));

  if (e == NULL) {
    /* memory subsystem hasn't been initialized yet (happens sometimes
       with threaded programs) */
    TMSG(EPOCH, "new epoch skipped (memory not initialized)");
    return NULL;
  }
  csprof_epoch_init(e);
  
  return e;
}


void
hpcrun_finalize_current_epoch(void)
{
  // lazily finalize the last epoch
  csprof_epoch_lock();
  if (current_epoch->loaded_modules == NULL) {
    fnbounds_epoch_finalize();
  }
  csprof_epoch_unlock();
}

void
hpcrun_epoch_reset(void)
{
  csprof_state_t *state = csprof_get_state();

  csprof_cct__init(&state->csdata, state->csdata_ctxt); // reset cct
}
