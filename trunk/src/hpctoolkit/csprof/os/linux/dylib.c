//***********************************************************************************
// system includes
//***********************************************************************************

#include <link.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>



//***********************************************************************************
// local includes
//***********************************************************************************

#include "dl_bound.h"
#include "pmsg.h"



//***********************************************************************************
// type declarations
//***********************************************************************************

struct dylib_seg_bounds_s {
  unsigned long long start;
  unsigned long long end;
};

struct dylib_fmca_s {
  unsigned long long addr;
  const char *module_name;
  struct dylib_seg_bounds_s bounds;
};



//***********************************************************************************
// macro declarations
//***********************************************************************************

#define SEG_START_ADDR(info, seg) \
    ((char *) (info)->dlpi_addr + (info)->dlpi_phdr[seg].p_vaddr)

#define SEG_SIZE(info, seg) ((info)->dlpi_phdr[seg].p_memsz)

#define SEG_IS_EXECUTABLE(info, seg) ((info)->dlpi_phdr[seg].p_flags & PF_X)



//***********************************************************************************
// forward declarations
//***********************************************************************************

static int dylib_note_dlopen_callback(struct dl_phdr_info *info, 
	                              size_t size, 
				      void *module_name_v);

static int dylib_find_module_containing_addr_callback(struct dl_phdr_info *info, 
						      size_t size, 
                                                      void *fargs_v);



//***********************************************************************************
// interface operations
//***********************************************************************************

//-------------------------------------------------------------
// postprocess a module after dynamic loading
//-------------------------------------------------------------
void 
dylib_note_dlopen(const char *module_name)
{
  dl_iterate_phdr(dylib_note_dlopen_callback, (void *) module_name);
}


int 
dylib_find_module_containing_addr(unsigned long long addr, 
				  char *module_name,
				  unsigned long long *start, 
				  unsigned long long *end)
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



//***********************************************************************************
// private operations
//***********************************************************************************

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

  bounds->start = (unsigned long long) start;
  bounds->end = (unsigned long long) end;
}


static int
dylib_note_dlopen_callback(struct dl_phdr_info *info, 
			   size_t size, 
			   void *module_name_v)
{
  const char *module_name = (char *) module_name_v;

  TMSG(DL_ADD_MODULE,"name in = %s, name considered = %s", module_name,
       info->dlpi_name);

  //------------------------------------------------------------------------
  // if the segment name matches the name supplied ...
  //------------------------------------------------------------------------
  if (strstr(info->dlpi_name, module_name)) {

    char resolved_path[PATH_MAX];
    struct dylib_seg_bounds_s bounds;

    dylib_get_segment_bounds(info, &bounds);
    realpath(info->dlpi_name, resolved_path);

    TMSG(ADD_MODULE_BASE,"name in:%s => %s, start = %p, end = %p", module_name,
	 resolved_path, bounds.start, bounds.end);

    dl_add_module_base(resolved_path, (char *) bounds.start, (char *) bounds.end);
    return 1;
  }

  return 0;
}


static int
dylib_find_module_containing_addr_callback(struct dl_phdr_info *info, 
					   size_t size, 
					   void *fargs_v)
{
  struct dylib_fmca_s *fargs = (struct dylib_fmca_s *) fargs_v;
  dylib_get_segment_bounds(info, &fargs->bounds);

  //------------------------------------------------------------------------
  // if addr is in within the segment bounds
  //------------------------------------------------------------------------
  if (fargs->addr >= (long long) fargs->bounds.start && fargs->addr >= (long long) fargs->bounds.end) {
    fargs->module_name = info->dlpi_name;
    return 1;
  }

  return 0;
}
