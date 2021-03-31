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
// Copyright ((c)) 2002-2021, Rice University
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
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


//#include <stdlib.h>
//#include <unistd.h>

#include	<elf.h>
#include	<libelf.h>
#include	<gelf.h>


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

// TODO:
// We will need to refactor this code to make the NUM_FNS a variable
// and the table of functions to be looked up can be a linked list,
// where any GPU can indicate that its functions should be added to
// the module ignore map when that type of GPU is being monitored.

#define NUM_FNS 10



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

static const char *IGNORE_FNS[NUM_FNS] = {
  "cuLaunchKernel",
  "cudaLaunchKernel",
  "cuptiActivityEnable",
  "roctracer_set_properties",  // amd roctracer library
  "amd_dbgapi_initialize",     // amd debug library
  "hipKernelNameRefByPtr",     // amd hip runtime
  "hsa_queue_create",          // amd hsa runtime
  "hpcrun_malloc",             // hpcrun library
  "clIcdGetPlatformIDsKHR",    // libigdrcl.so(intel opencl)
  "zeKernelCreate"             // libze_intel_gpu.so (intel L0) ISSUE: not getting ignored
};
static module_ignore_entry_t modules[NUM_FNS];
static pfq_rwlock_t modules_lock;

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

int
serach_functions_in_module(Elf *e, GElf_Shdr* secHead, Elf_Scn *section)
{
  Elf_Data *data;
  char *symName;
  uint64_t count;
  GElf_Sym curSym;
  uint64_t i, ii,symType, symBind;
  // char *marmite;

  data = elf_getdata(section, NULL);           // use it to get the data
  if (data == NULL || secHead->sh_entsize == 0) return -1;
  count = (secHead->sh_size)/(secHead->sh_entsize);
  for (ii=0; ii<count; ii++) {
    gelf_getsym(data, ii, &curSym);
    symName = elf_strptr(e, secHead->sh_link, curSym.st_name);
    symType = GELF_ST_TYPE(curSym.st_info);
    symBind = GELF_ST_BIND(curSym.st_info);

    // the .dynsym section can contain undefined symbols that represent imported symbols.
    // We need to find functions defined in the module.
    if ( (symType == STT_FUNC) && (symBind == STB_GLOBAL) && (curSym.st_value != 0)) {
      for (i = 0; i < NUM_FNS; ++i) {
        if (modules[i].empty && (strcmp(symName, IGNORE_FNS[i]) == 0)) {
          return i;
        }
      }
    }
	}
  return -1;
}

bool
module_ignore_map_ignore
(
  load_module_t* lm
)
{
  // Update path
  // Only one thread could update the flag,
  // Guarantee dlopen modules before notification are updated.
  bool result = false;
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&modules_lock, &me);


  load_module_t *module = lm;

  if (!pseudo_module_p(module->name)) {
    // Ignore the module if cannot resolve the path
    char resolved_path[PATH_MAX];
    if (realpath(module->name, resolved_path) == NULL) {
      pfq_rwlock_write_unlock(&modules_lock, &me);
      return result;
    }

    int fd = open (resolved_path, O_RDONLY);
    if (fd < 0) {
      pfq_rwlock_write_unlock(&modules_lock, &me);
      return false;
    }
    struct stat stat;
    if (fstat (fd, &stat) < 0) {
      close(fd);
      pfq_rwlock_write_unlock(&modules_lock, &me);
      return false;
    }

    char* buffer = (char*) mmap (NULL, stat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (buffer == NULL) {
      close(fd);
      pfq_rwlock_write_unlock(&modules_lock, &me);
      return false;
    }
    elf_version(EV_CURRENT);
    Elf *elf = elf_memory(buffer, stat.st_size);
    Elf_Scn *scn = NULL;
    GElf_Shdr secHead;

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
      gelf_getshdr(scn, &secHead);
      // Only search .dynsym section
      if (secHead.sh_type != SHT_DYNSYM) continue;
      int module_ignore_index = serach_functions_in_module(elf, &secHead, scn);
      if (module_ignore_index != -1) {
        modules[module_ignore_index].module = module;
        modules[module_ignore_index].empty = false;
        result = true;
        break;
      }
    }
    munmap(buffer, stat.st_size);
    close(fd);
  }
  pfq_rwlock_write_unlock(&modules_lock, &me);
  return result;
}


bool
module_ignore_map_delete
(
 load_module_t* lm
)
{
  size_t i;
  bool result = false;
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&modules_lock, &me);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false && modules[i].module == lm) {
      modules[i].empty = true;
      modules[i].module = NULL;
      result = true;
      break;
    }
  }
  pfq_rwlock_write_unlock(&modules_lock, &me);
  return result;
}
