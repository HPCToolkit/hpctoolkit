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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdlib.h>
#include <string.h>
#include <limits.h>

//#define GNU_SOURCE
#include <link.h>  // dl_iterate_phdr
#include <dlfcn.h> // dladdr


//*****************************************************************************
// local includes
//*****************************************************************************

#include "dylib.h"
#include "fnbounds_interface.h"

#include <messages/messages.h>


//*****************************************************************************
// type declarations
//*****************************************************************************

struct dylib_seg_bounds_s {
  void *start;
  void *end;
};

struct dylib_fmca_s {
  void *addr;
  const char *module_name;
  struct dylib_seg_bounds_s bounds;
};



//*****************************************************************************
// macro declarations
//*****************************************************************************

#define SEG_START_ADDR(info, seg) \
    ((char *) (info)->dlpi_addr + (info)->dlpi_phdr[seg].p_vaddr)

#define SEG_SIZE(info, seg) ((info)->dlpi_phdr[seg].p_memsz)

#define SEG_IS_EXECUTABLE_CODE(info, seg)	\
    (((info) != NULL) &&		\
     ((info)->dlpi_phdr != NULL) &&	\
     ((info)->dlpi_phdr[seg].p_type == PT_LOAD) && \
     ((info)->dlpi_phdr[seg].p_flags & PF_X))


//*****************************************************************************
// forward declarations
//*****************************************************************************

static int 
dylib_map_open_dsos_callback(struct dl_phdr_info *info, 
			     size_t size, void *);

static int 
dylib_find_module_containing_addr_callback(struct dl_phdr_info *info, 
					   size_t size, void *fargs_v);



//*****************************************************************************
// interface operations
//*****************************************************************************

//------------------------------------------------------------------
// ensure bounds information computed for all open shared libraries
//------------------------------------------------k-----------------
void 
dylib_map_open_dsos()
{
  dl_iterate_phdr(dylib_map_open_dsos_callback, (void *)0);
}


//------------------------------------------------------------------
// ensure bounds information computed for the executable
//------------------------------------------------------------------

void 
dylib_map_executable()
{
  const char *executable_name = "/proc/self/exe";
  fnbounds_ensure_mapped_dso(executable_name, NULL, NULL);
}


int 
dylib_addr_is_mapped(void *addr) 
{
  struct dylib_fmca_s arg;

  // initialize arg structure
  arg.addr = addr;
  return dl_iterate_phdr(dylib_find_module_containing_addr_callback, &arg);
}


int 
dylib_find_module_containing_addr(void* addr, 
				  char* module_name,
				  void** start, 
				  void** end)
{
  int retval = 0; // not found
  struct dylib_fmca_s arg;

  // initialize arg structure
  arg.addr = addr;

  if (dl_iterate_phdr(dylib_find_module_containing_addr_callback, &arg)) {

    //-------------------------------------
    // return callback results into arguments
    //-------------------------------------
    strcpy(module_name, arg.module_name);
    *start = arg.bounds.start;
    *end = arg.bounds.end;
    retval = 1;
  }

  return retval;
}


int 
dylib_find_proc(void* pc, void* *proc_beg, void* *mod_beg)
{
  Dl_info dli;
  int ret = dladdr(pc, &dli); // cf. glibc's _dl_addr
  if (ret) {
    //printf("dylib_find_proc: lm: %s (%p); sym: %s (%p)\n", dli.dli_fname, dli.dli_fbase, dli.dli_sname, dli.dli_saddr);
    *proc_beg = dli.dli_saddr;
    *mod_beg  = dli.dli_fbase;
    return 0;
  }
  else {
    *proc_beg = NULL;
    *mod_beg = NULL;
    return -1;
  }
}


#if defined(__ia64__) && defined(__linux__)
bool
dylib_isin_start_func(void* pc)
{
  extern int __libc_start_main(void); // start of a process                                                                             
  extern int __clone2(int (*fn)(void *),  void *child_stack_base,
                      size_t stack_size, int flags, void *arg, ...
                      /* pid_t *pid, struct user_desc *tls, pid_t *ctid */ );
  void* proc_beg = NULL, *mod_beg = NULL;
  dylib_find_proc(pc, &proc_beg, &mod_beg);

  return (proc_beg == __libc_start_main ||
          proc_beg == __clone2);
}
#else
bool
dylib_isin_start_func(void* pc)
{
  extern int __libc_start_main(void); // start of a process
  extern int __clone(void);           // start of a thread (extern)
  extern int clone(void);             // start of a thread (weak)

  void* proc_beg = NULL, *mod_beg = NULL;
  dylib_find_proc(pc, &proc_beg, &mod_beg);
  return (proc_beg == __libc_start_main || 
	  proc_beg == clone || proc_beg == __clone);
}
#endif  // __ia64__ && __linux__

const char* 
dylib_find_proc_name(const void* pc)
{
  Dl_info dli;
  int ret = dladdr(pc, &dli);
  if (ret) {
    return (dli.dli_sname) ? dli.dli_sname : dli.dli_fname;
  }
  else {
    return NULL;
  }
}



//*****************************************************************************
// private operations
//*****************************************************************************

void 
dylib_get_segment_bounds(struct dl_phdr_info *info, 
			 struct dylib_seg_bounds_s *bounds)
{
  int j;
  char *start = (char *) -1;
  char *end   = NULL;

  //------------------------------------------------------------------------
  // compute start of first & end of last executable chunks in this segment
  //------------------------------------------------------------------------
  for (j = 0; j < info->dlpi_phnum; j++) {
    if (SEG_IS_EXECUTABLE_CODE(info, j)) {
      char *saddr = SEG_START_ADDR(info, j);
      long size = SEG_SIZE(info, j);
      // don't adjust info unless segment has positive size
      if (size > 0) { 
	char *eaddr = saddr + size;
	if (saddr < start) start = saddr;
	if (eaddr >= end) end = eaddr;
      }
    }
  }

  bounds->start = start;
  bounds->end = end;
}


static int
dylib_map_open_dsos_callback(struct dl_phdr_info *info, size_t size, 
			     void *unused)
{
  if (strcmp(info->dlpi_name,"") != 0) {
    struct dylib_seg_bounds_s bounds;
    dylib_get_segment_bounds(info, &bounds);
    fnbounds_ensure_mapped_dso(info->dlpi_name, bounds.start, bounds.end);
  }

  return 0;
}


static int
dylib_find_module_containing_addr_callback(struct dl_phdr_info* info, 
					   size_t size, void* fargs_v)
{
  struct dylib_fmca_s* fargs = (struct dylib_fmca_s*) fargs_v;
  dylib_get_segment_bounds(info, &fargs->bounds);

  //------------------------------------------------------------------------
  // if addr is in within the segment bounds
  //------------------------------------------------------------------------
  if (fargs->addr >= fargs->bounds.start && 
      fargs->addr < fargs->bounds.end) {
    fargs->module_name = info->dlpi_name;
    return 1;
  }

  return 0;
}

