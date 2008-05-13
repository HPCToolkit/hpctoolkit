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
#include "interface.h"
#include "epoch.h"
#include "mem.h"
#include "csprof_csdata.h"
#include "general.h"
#include "hpcfile_general.h"
#include "spinlock.h"
#include "state.h"

/* epochs are entirely separate from profiling state */
static csprof_epoch_t *current_epoch = NULL;

/* locking functions to ensure that epochs are consistent */
static spinlock_t epoch_lock = 0;

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
csprof_epoch_add_module(const char *module_name, 
	void *vaddr,                /* the preferred virtual address */
  	void *mapaddr,              /* the actual mapped address */
        size_t size)                /* just what it sounds like */
{
  csprof_epoch_module_t *m = (csprof_epoch_module_t *) csprof_malloc(sizeof(csprof_epoch_module_t));

  // fill in the fields of the structure
  m->module_name = module_name;
  m->vaddr = vaddr;
  m->mapaddr = mapaddr;
  m->size = size;

  // link it into the list of loaded modules for the current epoch
  m->next = current_epoch->loaded_modules;
  current_epoch->loaded_modules = m->next; 
}


/* epochs are totally distinct from profiling states */
csprof_epoch_t *
csprof_epoch_new()
{
  csprof_epoch_t *e = csprof_malloc(sizeof(csprof_epoch_t));

  if(e == NULL) {
    /* memory subsystem hasn't been initialized yet (happens sometimes
       with threaded programs) */
    return NULL;
  }

  memset(e, 0, sizeof(*e));

  /* initialize the modules list */
  csprof_epoch_get_loaded_modules(e, current_epoch);

  /* make sure everything is up-to-date before setting the
     current_epoch */
  e->next = current_epoch;
  current_epoch = e;

  return e;
}

/* writing epochs to disk */

#define HPCFILE_EPOCH_MAGIC_STR     "HPC_EPOCH"
#define HPCFILE_EPOCH_MAGIC_STR_LEN  9 /* exclude '\0' */
#define HPCFILE_EPOCH_ENDIAN 'l' /* 'l' for little, 'b' for big */

void
csprof_write_epoch_header(FILE *fs)
{
}

void
csprof_write_epoch_module(csprof_epoch_module_t *module, FILE *fs)
{
  uint32_t namelen = strlen(module->module_name);

  /* write the string */
  hpc_fwrite_le4(&namelen, fs);
  fwrite(module->module_name, sizeof(char), namelen, fs);

  /* write the appropriate addresses */
  hpc_fwrite_le8((uint64_t*)&module->vaddr, fs);
  hpc_fwrite_le8((uint64_t*)&module->mapaddr, fs);
}

void
csprof_write_epoch(csprof_epoch_t *epoch, FILE *fs)
{
  csprof_epoch_module_t *module_runner;

  hpc_fwrite_le4(&epoch->num_modules, fs);

  module_runner = epoch->loaded_modules;

  while(module_runner != NULL) {
    MSG(CSPROF_MSG_DATAFILE, "Writing module %s", module_runner->module_name);
    csprof_write_epoch_module(module_runner, fs);

    module_runner = module_runner->next;
  }
}

void
csprof_write_all_epochs(FILE *fs)
{
  /* go through and assign all epochs an id; this id will be used by
     the callstack data trees to indicate in which epoch they were
     collected. */
  csprof_epoch_t *runner = current_epoch;
  unsigned int id_runner = 0;

  while(runner != NULL) {
    runner->id = id_runner;

    id_runner++;
    runner = runner->next;
  }

  /* indicate how many epochs there are to read */
  hpc_fwrite_le4(&id_runner, fs);

  runner = current_epoch;

  while(runner != NULL) {
    MSG(CSPROF_MSG_DATAFILE, "Writing epoch %d", runner->id);
    csprof_write_epoch(runner, fs);

    runner = runner->next;
  }
}

#ifdef NOTYET
csprof_state_t *
csprof_check_for_new_epoch(csprof_state_t *state)
{
  /* ugh, nasty race condition here:

  1. shared library state has changed since the last profile
  signal, so we enter the if;

  2. somebody else dlclose()'s a library which holds something
  located in our backtrace.  this is not in itself a problem,
  since we don't bother doing anything on dlclose()...;

  3. somebody else (thread in step 2 or a different thread)
  dlopen()'s a new shared object, which begins an entirely
  new epoch--one which does not include the shared object
  which resides in our backtrace;

  4. we create a new state which receives the epoch from step 3,
  not step 1, which is wrong.

  attempt to take baby steps to stop this.  more drastic action
  would involve grabbing the epoch lock, but I believe that would
  be unacceptably slow (both in the atomic instruction overhead
  and the simple fact that most programs are not frequent users
  of dl*). */

  csprof_epoch_t *current = csprof_get_epoch();

  if(state->epoch != current) {
    csprof_state_t *newstate = csprof_malloc(sizeof(csprof_state_t));

    MSG(CSPROF_MSG_EPOCH, "Creating new epoch...");

    /* we don't have to go through the usual csprof_state_{init,alloc}
       business here because most of the stuff we want is already
       in `state' */
    memcpy(newstate, state, sizeof(csprof_state_t));

    /* we do have to reinitialize the tree, though */
    csprof_csdata__init(&newstate->csdata);

    /* and reinsert backtraces */
    if(newstate->bufend - newstate->bufstk != 0) {
      newstate->treenode = NULL;
      csprof_state_insert_backtrace(newstate, 0, /* pick one */
                                    newstate->bufend - 1,
                                    newstate->bufstk,
                                    0);
    }

    /* and inform the state about its epoch */
    newstate->epoch = current;

    /* and finally, set the new state */
    csprof_set_state(newstate);

#ifdef CSPROF_THREADS
    // csprof_duplicate_thread_state_reference(newstate);
    ;
#endif

    return newstate;
  }
  else {
    return state;
  }
}
#endif
