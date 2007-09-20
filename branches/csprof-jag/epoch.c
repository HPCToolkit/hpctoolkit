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
#include "atomic.h"

/* epochs are entirely separate from profiling state */
static csprof_epoch_t *current_epoch = NULL;


/* locking functions to ensure that epochs are consistent */
static volatile long epoch_lock = 0;

void
csprof_epoch_lock()
{
    /* implement our own spinlocks, since the lock has to be tested in
       the signal handler and we're not allowed to call pthread locking
       functions while we're in the signal handler. */
    while(csprof_atomic_exchange_pointer(&epoch_lock, 1)) {
        while(epoch_lock == 1) ; /* spin efficiently */
    }
}

void
csprof_epoch_unlock()
{
    /* FIXME: does this *need* to be atomic? */
    csprof_atomic_decrement(&epoch_lock);
}

int
csprof_epoch_is_locked()
{
    extern int s2;
    s2 += 1;
    MSG(1,"epoch lock val = %lx",epoch_lock);
    return epoch_lock;
}

csprof_epoch_t *
csprof_get_epoch()
{
    return current_epoch;
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
    size_t namelen = strlen(module->module_name);

    /* write the string */
    hpc_fwrite_le4(&namelen, fs);
    fwrite(module->module_name, sizeof(char), namelen, fs);

    /* write the appropriate addresses */
    hpc_fwrite_le8(&module->vaddr, fs);
    hpc_fwrite_le8(&module->mapaddr, fs);
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
