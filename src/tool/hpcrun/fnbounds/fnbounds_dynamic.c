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
// Copyright ((c)) 2002-2016, Rice University
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


#include <include/hpctoolkit-config.h>

//*********************************************************************
// external libraries
//*********************************************************************

#include <monitor.h>

//*********************************************************************
// local includes
//*********************************************************************

#include "fnbounds_interface.h"
#include "fnbounds_file_header.h"
#include "client.h"
#include "dylib.h"

#include <hpcrun/main.h>
#include <hpcrun_dlfns.h>
#include <hpcrun_stats.h>
#include <disabled.h>
#include <loadmap.h>
#include <epoch.h>
#include <sample_event.h>
#include <thread_data.h>

#include <unwind/common/ui_tree.h>
#include <messages/messages.h>

#include <lib/prof-lean/spinlock.h>


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

#define FNBOUNDS_LOCK  do {			\
	spinlock_lock(&fnbounds_lock);		\
	TD_GET(fnbounds_lock) = 1;		\
} while (0)

#define FNBOUNDS_UNLOCK  do {			\
	spinlock_unlock(&fnbounds_lock);	\
	TD_GET(fnbounds_lock) = 0;		\
} while (0)


//*********************************************************************
// forward declarations
//*********************************************************************

static load_module_t *
fnbounds_get_loadModule(void *ip);

static dso_info_t *
fnbounds_compute(const char *filename, void *start, void *end);

static void
fnbounds_map_executable();


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
fnbounds_init()
{
  if (hpcrun_get_disabled()) return 0;

  hpcrun_syserv_init();
  fnbounds_map_executable();
  fnbounds_map_open_dsos();

  return 0;
}

bool
fnbounds_enclosing_addr(void* ip, void** start, void** end, load_module_t** lm)
{
  FNBOUNDS_LOCK;

  bool ret = false; // failure unless otherwise reset to 0 below
  
  load_module_t* lm_ = fnbounds_get_loadModule(ip);
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

  FNBOUNDS_UNLOCK;

  return ret;
}


//---------------------------------------------------------------------
// Function: fnbounds_map_open_dsos
// Purpose:  
//     identify any new dsos that have been mapped.
//     analyze them and add their information to the open list.
//---------------------------------------------------------------------

void
fnbounds_map_open_dsos()
{
  dylib_map_open_dsos();
}


//
// Find start and end of executable from /proc/self/maps
//
static void
fnbounds_find_exec_bounds_proc_maps(char* exename, void**start, void** end)
{
  *start = NULL; *end = NULL;
  FILE* loadmap = fopen("/proc/self/maps", "r");
  if (! loadmap) {
    EMSG("Could not open /proc/self/maps");
    return;
  }
  char linebuf[1024 + 1];
  char tmpname[PATH_MAX];
  char* addr = NULL;
  for(;;) {
    char* l = fgets(linebuf, sizeof(linebuf), loadmap);
    if (feof(loadmap)) break;
    char* save = NULL;
    const char delim[] = " \n";
    addr = strtok_r(l, delim, &save);
    char* perms = strtok_r(NULL, delim, &save);
    // skip 3 tokens
    for (int i=0; i < 3; i++) { (void) strtok_r(NULL, delim, &save);}
    char* name = strtok_r(NULL, delim, &save);
    realpath(name, tmpname); 
    if ((strncmp(perms, "r-x", 3) == 0) && (strcmp(tmpname, exename) == 0)) break;
  }
  fclose(loadmap);
  char* save = NULL;
  const char dash[] = "-";
  char* start_str = strtok_r(addr, dash, &save);
  char* end_str   = strtok_r(NULL, dash, &save);
  *start = (void*) (uintptr_t) strtol(start_str, NULL, 16);
  *end   = (void*) (uintptr_t) strtol(end_str, NULL, 16);
}

dso_info_t*
fnbounds_dso_exec(void)
{
  char filename[PATH_MAX];
  struct fnbounds_file_header fh;
  void* start = NULL;
  void* end   = NULL;

  TMSG(MAP_EXEC, "Entry");
  realpath("/proc/self/exe", filename);
  void** nm_table = (void**) hpcrun_syserv_query(filename, &fh);
  if (! nm_table) {
    EMSG("No nm_table for executable %s", filename);
    return hpcrun_dso_make(filename, NULL, NULL, NULL, NULL, 0);
  }
  if (fh.num_entries < 1) {
    EMSG("fnbounds returns no symbols for file %s, (all intervals poisoned)", filename);
    return hpcrun_dso_make(filename, NULL, NULL, NULL, NULL, 0);
  }
  TMSG(MAP_EXEC, "Relocatable exec");
  if (fh.is_relocatable) {
    if (nm_table[0] >= start && nm_table[0] <= end) {
      // segment loaded at its preferred address
      fh.is_relocatable = 0;
    }
    // Use loadmap to find start, end for a relocatable executable
    fnbounds_find_exec_bounds_proc_maps(filename, &start, &end);
    TMSG(MAP_EXEC, "Bounds for relocatable exec = %p, %p", start, end);
  }
  else {
    TMSG(MAP_EXEC, "NON relocatable exec");
    char executable_name[PATH_MAX];
    void* mstart; 
    void* mend;
    if (dylib_find_module_containing_addr(nm_table[0],
					  executable_name, &mstart, &mend)) {
      start = (void*) mstart;
      end = (void*) mend;
    }
    else {
      start = nm_table[0];
      end = nm_table[fh.num_entries - 1];
    }
  }
  return hpcrun_dso_make(filename, nm_table, &fh, start, end, fh.mmap_size);
}

bool
fnbounds_ensure_mapped_dso(const char *module_name, void *start, void *end)
{
  bool isOk = true;

  FNBOUNDS_LOCK;

  load_module_t *lm = hpcrun_loadmap_findByAddr(start, end);
  if (!lm) {
    dso_info_t *dso = fnbounds_compute(module_name, start, end);
    if (dso) {
      hpcrun_loadmap_map(dso);
    }
    else {
      EMSG("!! INTERNAL ERROR, not possible to map dso for %s (%p, %p)",
	   module_name, start, end);
      isOk = false;
    }
  }

  FNBOUNDS_UNLOCK;

  return isOk;
}


//---------------------------------------------------------------------
// Function: fnbounds_unmap_closed_dsos
// Purpose:  
//     identify any dsos that are no longer mapped.
//     move them from the open to the closed list.
//---------------------------------------------------------------------

void
fnbounds_unmap_closed_dsos()
{
  FNBOUNDS_LOCK;

  TMSG(LOADMAP, "Unmapping closed dsos");
  load_module_t *current = hpcrun_getLoadmap()->lm_head;
  while (current) {
    if (current->dso_info) {
      if (!dylib_addr_is_mapped(current->dso_info->start_addr)) {
        TMSG(LOADMAP, "Unmapping %s", current->name);
        hpcrun_loadmap_unmap(current);
      }
    }
    current = current->next;
  }

  FNBOUNDS_UNLOCK;
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

  hpcrun_syserv_fini();
}


void
fnbounds_release_lock(void)
{
  FNBOUNDS_UNLOCK;  
}


fnbounds_table_t
fnbounds_fetch_executable_table(void)
{
  char exename[PATH_MAX];
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
  struct fnbounds_file_header fh;
  char filename[PATH_MAX];
  void** nm_table;
  long map_size;

  if (incoming_filename == NULL) {
    return (NULL);
  }

  // linux-vdso.so and linux-gate.so are virtual files and don't exist
  // in the file system.
  if (strncmp(incoming_filename, "linux-vdso.so", 13) == 0
      || strncmp(incoming_filename, "linux-gate.so", 13) == 0) {
    return hpcrun_dso_make(incoming_filename, NULL, NULL, start, end, 0);
  }

  realpath(incoming_filename, filename);

  nm_table = (void**) hpcrun_syserv_query(filename, &fh);
  if (nm_table == NULL) {
    return hpcrun_dso_make(filename, NULL, NULL, start, end, 0);
  }
  map_size = fh.mmap_size;

  if (fh.num_entries < 1) {
    EMSG("fnbounds returns no symbols for file %s, (all intervals poisoned)", filename);
    return hpcrun_dso_make(filename, NULL, NULL, start, end, 0);
  }

  //
  // Note: we no longer care if binary is stripped.
  //
  if (fh.is_relocatable) {
    if (nm_table[0] >= start && nm_table[0] <= end) {
      // segment loaded at its preferred address
      fh.is_relocatable = 0;
    }
  }
  else {
    char executable_name[PATH_MAX];
    void *mstart; 
    void *mend;
    if (dylib_find_module_containing_addr(nm_table[0],
					  executable_name, &mstart, &mend)) {
      start = (void*) mstart;
      end = (void*) mend;
    }
    else {
      start = nm_table[0];
      end = nm_table[fh.num_entries - 1];
    }
  }

  return hpcrun_dso_make(filename, nm_table, &fh, start, end, map_size);
}


// fnbounds_get_loadModule(): Given the (unnormalized) IP 'ip',
// attempt to return the enclosing load module.  Note that the
// function may fail.
static load_module_t *
fnbounds_get_loadModule(void *ip)
{
  load_module_t* lm = hpcrun_loadmap_findByAddr(ip, ip);
  dso_info_t* dso = (lm) ? lm->dso_info : NULL;

  // We can't call dl_iterate_phdr() in general because catching a
  // sample at just the wrong point inside dlopen() will segfault or
  // deadlock.
  //
  // However, the risk is small, and if we're willing to take the
  // risk, then analyzing the new DSO here allows us to sample inside
  // an init constructor.
  if (!dso && ENABLED(DLOPEN_RISKY) && hpcrun_dlopen_pending() > 0) {
    char module_name[PATH_MAX];
    void *mstart, *mend;
    
    if (dylib_find_module_containing_addr(ip, module_name, &mstart, &mend)) {
      dso = fnbounds_compute(module_name, mstart, mend);
      if (dso) {
	lm = hpcrun_loadmap_map(dso);
      }
    }
  }
  
  return lm;
}


static void
fnbounds_map_executable()
{
  // dylib_map_executable() ==>
  // fnbounds_ensure_mapped_dso("/proc/self/exe", NULL, NULL) ==>
  //{
  //   FNBOUNDS_LOCK;
  //   dso = fnbound_compute(exename, ...);
  //   hpcrun_loadmap_map(dso);
  //   FNBOUNDS_UNLOCK;
  //}
  FNBOUNDS_LOCK;
  hpcrun_loadmap_map(fnbounds_dso_exec());
  FNBOUNDS_UNLOCK;
}
