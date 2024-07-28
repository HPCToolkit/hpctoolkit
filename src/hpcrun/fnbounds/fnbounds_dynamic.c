// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//=====================================================================
// File: fnbounds_dynamic.c
//
//     provide information about function bounds for functions in
//     dynamically linked load modules. use an extra "server" process
//     to handle computing the symbols to insulate the client process
//     from the complexity of this task, including use of the system
//     command to fork new processes. having the server process
//     enables use to avoid dealing with forking off new processes
//     with system when there might be multiple threads active with
//     sampling enabled.
//
//  Modification history:
//     2008 April 28 - created John Mellor-Crummey
//
//=====================================================================


//*********************************************************************
// system includes
//*********************************************************************

#define _GNU_SOURCE

#include <stdio.h>     // fopen, fclose, etc
#include <dlfcn.h>     // for dlopen/dlclose
#include <string.h>    // for strcmp, strerror
#include <stdlib.h>    // for getenv
#include <errno.h>     // for errno
#include <stdint.h>
#include <stdbool.h>
#include <sys/param.h> // for PATH_MAX
#include <sys/types.h>
#include <unistd.h>    // getpid


//*********************************************************************
// external libraries
//*********************************************************************

#include "../libmonitor/monitor.h"

//*********************************************************************
// local includes
//*********************************************************************

#include "fnbounds_interface.h"
#include "fnbounds.h"

#include "../main.h"
#include "../hpcrun_stats.h"
#include "../disabled.h"
#include "../files.h"
#include "../loadmap.h"
#include "../epoch.h"
#include "../sample_event.h"
#include "../thread_data.h"

#include "../unwind/common/uw_recipe_map.h"
#include "../messages/messages.h"

#include "../../common/lean/spinlock.h"
#include "../../common/lean/vdso.h"
#include "../../common/lean/crypto-hash.h"



//*********************************************************************
// local types
//*********************************************************************

#define PERFORM_RELOCATION(addr, offset) \
        ((void *) (((unsigned long) addr) + ((long) offset)))

#define MAPPING_END(addr, length) \
        ((void *) (((unsigned long) addr) + ((unsigned long) length)))


//*********************************************************************
// local variables
//*********************************************************************

// locking functions to ensure that dynamic bounds data structures
// are consistent.

static spinlock_t fnbounds_lock = SPINLOCK_UNLOCKED;

#define FNBOUNDS_LOCK  do {                     \
        spinlock_lock(&fnbounds_lock);          \
        TD_GET(fnbounds_lock) = 1;              \
} while (0)

#define FNBOUNDS_UNLOCK  do {                   \
        spinlock_unlock(&fnbounds_lock);        \
        TD_GET(fnbounds_lock) = 0;              \
} while (0)


//*********************************************************************
// forward declarations
//*********************************************************************

static dso_info_t *
fnbounds_compute(const char *filename, void *start, void *end);


//*********************************************************************
// interface operations
//*********************************************************************

//---------------------------------------------------------------------
// function fnbounds_init:
//
//     for dynamically-linked executables, start an fnbounds server
//     process to that will compute function bounds information upon
//     demand for dynamically-linked load modules.
//
//     return code = 0 upon success, otherwise fork failed
//
//     NOTE: don't make this routine idempotent: it may be needed to
//     start a new server if the process forks
//---------------------------------------------------------------------

int
fnbounds_init(const char *executable_name)
{
  if (hpcrun_get_disabled()) return 0;

  return 0;
}

bool
fnbounds_enclosing_addr(void* ip, void** start, void** end, load_module_t** lm)
{
  bool ret = false; // failure unless otherwise reset to 0 below

  load_module_t* lm_ = hpcrun_loadmap_findByAddr(ip, ip);
  dso_info_t* dso = (lm_) ? lm_->dso_info : NULL;

  if (dso && dso->nsymbols > 0) {
    void* ip_norm = ip;
    if (dso->is_relocatable) {
      ip_norm = (void*) (((unsigned long) ip_norm) - dso->start_to_ref_dist);
    }

     // no dso table means no enclosing addr

    if (dso->table) {
      // N.B.: works on normalized IPs
      int rv = fnbounds_table_lookup(dso->table, dso->nsymbols, ip_norm,
                                     (void**) start, (void**) end);

      ret = (rv == 0);
      // Convert 'start' and 'end' into unnormalized IPs since they are
      // currently normalized.
      if (rv == 0 && dso->is_relocatable) {
        *start = PERFORM_RELOCATION(*start, dso->start_to_ref_dist);
        *end   = PERFORM_RELOCATION(*end  , dso->start_to_ref_dist);
      }
    }
  }

  if (lm) {
    *lm = lm_;
  }

  return ret;
}

load_module_t*
fnbounds_map_dso(const char *module_name, void *start, void *end, struct dl_phdr_info* info)
{
  dso_info_t *dso = fnbounds_compute(module_name, start, end);
  if (dso) {
    load_module_t* lm = hpcrun_loadmap_map(dso);
    lm->phdr_info = *info;
    return lm;
  }

  EMSG("!! INTERNAL ERROR, not possible to map dso for %s (%p, %p)",
       module_name, start, end);
  return NULL;
}

//---------------------------------------------------------------------
// function fnbounds_fini:
//
//     for dynamically-linked executables, shut down the fnbounds
//     server process
//---------------------------------------------------------------------

void
fnbounds_fini()
{
  if (hpcrun_get_disabled()) return;
}


void
fnbounds_release_lock(void)
{
  FNBOUNDS_UNLOCK;
}


fnbounds_table_t
fnbounds_fetch_executable_table(void)
{
  char exename[PATH_MAX + 1];
  realpath("/proc/self/exe", exename);
  TMSG(INTERVALS_PRINT, "name of loadmap = %s", exename);
  load_module_t* exe_lm = hpcrun_loadmap_findByName(exename);
  TMSG(INTERVALS_PRINT, "load module found = %p", exe_lm);
  if (!exe_lm) return (fnbounds_table_t) {.table = (void**) NULL, .len = 0};
  TMSG(INTERVALS_PRINT, "dso info for load module = %p", exe_lm->dso_info);
  if (! exe_lm->dso_info) return (fnbounds_table_t) {.table = (void**) NULL, .len = 0};
  return (fnbounds_table_t)
    { .table = exe_lm->dso_info->table, .len = exe_lm->dso_info->nsymbols};
}


//*********************************************************************
// private operations
//
// Note: the private operations should all assume that fnbounds_lock
// is already locked (mostly).
//*********************************************************************

static dso_info_t*
fnbounds_compute(const char* incoming_filename, void* start, void* end)
{
  char filename[PATH_MAX + 1];

  // typically, we use the filename for the query to the system server. however,
  // for [vdso], the filename will be the name of a file in the measurements
  // directory where a copy of the [vdso] segment will be saved. for parallel programs,
  // there is a race between the one process that opens the file first in exclusive
  // mode and other processes that just continue. one of the continuing processes
  // might try to invoke the system server to compute function bounds based on the
  // file contents before the file is completely written. for that reason, we use
  // declare below pathname_for_query, which will just be [vdso] rather than the
  // name of the file that contains a copy. given [vdso], the system server will
  // compute the bounds using its own memory-mapped copy of [vdso] rather than
  // waiting for the file to be written -- johnmc 7/2017
  const char *pathname_for_query;
  if (incoming_filename == NULL) {
    return (NULL);
  }

  // [vdso] and linux-gate.so are virtual files and don't exist
  // in the file system.
  // filename is recorded in returned dso_info_t as its name (relative path for vdso)
  // pathname_for_query is used in hpcrun_syserv_query (absolute path for vdso)
  if (strncmp(incoming_filename, "linux-gate.so", 13) == 0) {
    filename[sizeof(filename) - 1] = 0;
    strncpy(filename, incoming_filename, PATH_MAX);
    pathname_for_query = filename;
  } else if (strstr(incoming_filename, "/vdso/") != NULL && strstr(incoming_filename, ".vdso") != NULL) {
    char tmp_filename [PATH_MAX + 1];
    realpath(incoming_filename, tmp_filename);
    pathname_for_query = tmp_filename;
    if(strlen(incoming_filename) < 9 + CRYPTO_HASH_STRING_LENGTH){ //use realpath then
      strcpy(filename, tmp_filename);
    }else{
      strncpy(filename, &tmp_filename[strlen(incoming_filename) - 9 - CRYPTO_HASH_STRING_LENGTH], 10 + CRYPTO_HASH_STRING_LENGTH);
    }
  } else {
    realpath(incoming_filename, filename);
    pathname_for_query = filename;
  }

  FnboundsResponse fnbres = fnb_get_funclist(pathname_for_query);
  if (fnbres.entries == NULL) {
    return hpcrun_dso_make(filename, NULL, start, end);
  }

  if (fnbres.num_entries < 1) {
    EMSG("fnbounds returns no symbols for file %s, (all intervals poisoned)", filename);
    return hpcrun_dso_make(filename, NULL, start, end);
  }

  //
  // Note: we no longer care if binary is stripped.
  //
  if (fnbres.is_relocatable) {
    if (fnbres.entries[0] >= start && fnbres.entries[0] <= end) {
      // segment loaded at its preferred address
      fnbres.is_relocatable = 0;
    }
  }

  return hpcrun_dso_make(filename, &fnbres, start, end);
}
