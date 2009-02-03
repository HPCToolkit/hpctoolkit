// -*-Mode: C++;-*- // technically C99
// $Id$

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
#include "pmsg.h"



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

#define SEG_IS_EXECUTABLE(info, seg)	\
    (((info) != NULL) &&		\
     ((info)->dlpi_phdr != NULL) &&	\
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
  fnbounds_note_module(executable_name, NULL, NULL);
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
dylib_find_module_containing_addr(void *addr, 
				  char *module_name,
				  void **start, 
				  void **end)
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
    if (SEG_IS_EXECUTABLE(info, j)) {
      char *saddr = SEG_START_ADDR(info, j);
      char *eaddr = saddr + SEG_SIZE(info, j);
      if (saddr < start) start = saddr;
      if (eaddr >= end) end = eaddr;
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
    fnbounds_note_module(info->dlpi_name, bounds.start, bounds.end);
  }

  return 0;
}


static int
dylib_find_module_containing_addr_callback(struct dl_phdr_info *info, 
					   size_t size, void *fargs_v)
{
  struct dylib_fmca_s *fargs = (struct dylib_fmca_s *) fargs_v;
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

