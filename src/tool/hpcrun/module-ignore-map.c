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
// Copyright ((c)) 2002-2020, Rice University
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

//***************************************************************************
//
// File:
//   module-ignore-map.c
//
// Purpose:
//   implementation of a map of load modules that should be omitted
//   from call paths for synchronous samples
//   
//  
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************


#include <fcntl.h>   // open
#include <dlfcn.h>  // dlopen
#include <limits.h>  // PATH_MAX



//***************************************************************************
// local includes
//***************************************************************************

#include <monitor.h>  

#include <lib/prof-lean/pfq-rwlock.h>
#include <hpcrun/loadmap.h>

#include "module-ignore-map.h"



//***************************************************************************
// macros
//***************************************************************************

#define MODULE_IGNORE_DEBUG 0

#if MODULE_IGNORE_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define NUM_FNS 3



//***************************************************************************
// type declarations
//***************************************************************************

typedef struct module_ignore_entry {
  bool empty;
  load_module_t *module;
} module_ignore_entry_t;



//***************************************************************************
// static data
//***************************************************************************

static const char *NVIDIA_FNS[NUM_FNS] = {
  "cuLaunchKernel", "cudaLaunchKernel", "cuptiActivityEnable"
};
static module_ignore_entry_t modules[NUM_FNS];
static pfq_rwlock_t modules_lock;



//***************************************************************************
// private operations
//***************************************************************************

static bool
lm_contains_fn
(
 const char *lm, 
 const char *fn
) 
{
  char resolved_path[PATH_MAX];
  bool load_handle = false;

  char *lm_real = realpath(lm, resolved_path);
  void *handle = monitor_real_dlopen(lm_real, RTLD_NOLOAD);
  // At the start of a program, libs are not loaded by dlopen
  if (handle == NULL) {
    handle = monitor_real_dlopen(lm_real, RTLD_LAZY);
    load_handle = handle ? true : false;
  }
  PRINT("query path = %s\n", lm_real);
  PRINT("query fn = %s\n", fn);
  PRINT("handle = %p\n", handle);
  bool result = false;
  if (handle && lm_real) {
    void *fp = dlsym(handle, fn);
    PRINT("fp = %p\n", fp);
    if (fp) {
      Dl_info dlinfo;
      int res = dladdr(fp, &dlinfo);
      if (res) {
        char dli_fname_buf[PATH_MAX];
        char *dli_fname = realpath(dlinfo.dli_fname, dli_fname_buf);
        result = (strcmp(lm_real, dli_fname) == 0);
        PRINT("original path = %s\n", dlinfo.dli_fname);
        PRINT("found path = %s\n", dli_fname);
        PRINT("symbol = %s\n", dlinfo.dli_sname);
      }
    }
  }
  // Close the handle it is opened here
  if (load_handle) {
    monitor_real_dlclose(handle);
  }
  return result;
}


int
pseudo_module_p
(
  char *name
)
{
    // last character in the name
    char lastchar = 0;  // get the empty string case right

    while (*name) {
      lastchar = *name;
      name++;
    }

    // pseudo modules have [name] in /proc/self/maps
    // because we store [vdso] in hpctooolkit's measurement directory,
    // it actually has the name /path/to/measurement/directory/[vdso].
    // checking the last character tells us it is a virtual shared library.
    return lastchar == ']'; 
}



//***************************************************************************
// interface operations
//***************************************************************************

void
module_ignore_map_init
(
 void
)
{
  size_t i;
  for (i = 0; i < NUM_FNS; ++i) {
    modules[i].empty = true;
    modules[i].module = NULL;
  }
  pfq_rwlock_init(&modules_lock);
}


bool
module_ignore_map_module_id_lookup
(
 uint16_t module_id
)
{
  // Read path
  size_t i;
  bool result = false;
  pfq_rwlock_read_lock(&modules_lock);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false && modules[i].module->id == module_id) {
      /* current module should be ignored */
      result = true;
      break;
    }
  }
  pfq_rwlock_read_unlock(&modules_lock);
  return result;
}


bool
module_ignore_map_module_lookup
(
 load_module_t *module
)
{
  return module_ignore_map_lookup(module->dso_info->start_addr, 
				  module->dso_info->end_addr);
}


bool
module_ignore_map_inrange_lookup
(
 void *addr
)
{
  return module_ignore_map_lookup(addr, addr);
}


bool
module_ignore_map_lookup
(
 void *start, 
 void *end
)
{
  // Read path
  size_t i;
  bool result = false;
  pfq_rwlock_read_lock(&modules_lock);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false &&
      modules[i].module->dso_info->start_addr <= start &&
      modules[i].module->dso_info->end_addr >= end) {
      /* current module should be ignored */
      result = true;
      break;
    }
  }
  pfq_rwlock_read_unlock(&modules_lock);
  return result;
}


bool
module_ignore_map_ignore
(
 void *start, 
 void *end
)
{
  // Update path
  // Only one thread could update the flag,
  // Guarantee dlopen modules before notification are updated.
  size_t i;
  bool result = false;
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&modules_lock, &me);


  load_module_t *module = hpcrun_loadmap_findByAddr(start, end);

  if (!pseudo_module_p(module->name)) {
    // this is a real load module; let's see if it contains 
    // any of the nvidia functions
    for (i = 0; i < NUM_FNS; ++i) {
      if (modules[i].empty == true) {
	if (lm_contains_fn(module->name, NVIDIA_FNS[i])) {
	  modules[i].module = module;
	  modules[i].empty = false;
	  result = true;
	  break;
	}
      }
    }
  }

  pfq_rwlock_write_unlock(&modules_lock, &me);
  return result;
}


bool
module_ignore_map_delete
(
 void *start, 
 void *end
)
{
  size_t i;
  bool result = false;
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&modules_lock, &me);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false &&
      modules[i].module->dso_info->start_addr <= start &&
      modules[i].module->dso_info->end_addr >= end) {
      modules[i].empty = true;
      result = true;
      break;
    }
  }
  pfq_rwlock_write_unlock(&modules_lock, &me);
  return result;
}
