//===============================================
// File: fnbounds_dynamic.c  
// 
//     provide information about function bounds for functions in dynamically 
//     linked load modules. use an extra "server" process to handle computing the
//     symbols to insulate the client process from the complexity of this task, 
//     including use of the system command to fork new processes. having the
//     server process enables use to avoid dealing with forking off new processes
//     with system when there might be multiple threads active with sampling  
//     enabled.
//
//  Modification history:
//     2008 April 28 - created John Mellor-Crummey
//===============================================

//*******************************************************************************
// system includes 
//*******************************************************************************

#include <dlfcn.h>     // for dlopen/dlclose
#include <string.h>    // for strcmp, strerror
#include <stdlib.h>    // for getenv
#include <errno.h>     // for errno
#include <sys/param.h> // for PATH_MAX
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>    // getpid


//*******************************************************************************
// local includes 
//*******************************************************************************

#include "csprof_dlfns.h"
#include "dylib.h"
#include "epoch.h"
#include "fnbounds_interface.h"
#include "monitor.h"
#include "pmsg.h"
#include "structs.h"
#include "system_server.h"
#include "unlink.h"



/******************************************************************************
 * local types
 *****************************************************************************/

typedef struct dso_info_s {
  char *name;
  void *start_addr;
  void *end_addr;
  int relocate;

  void **table;
  int nsymbols;

  void *handle;

  struct dso_info_s *next, *prev;
} dso_info_t;

#define PERFORM_RELOCATION(addr, base) \
	((void *) (((unsigned long) addr) + ((unsigned long) base)))

#define MAPPING_END(addr, length) \
	((void *) (((unsigned long) addr) + ((unsigned long) length)))



/******************************************************************************
 * local variables
 *****************************************************************************/

// FIXME: tmproot should be overridable with an option.
static char *tmproot = "/tmp";

static char fnbounds_tmpdir[PATH_MAX];

static dso_info_t *dso_open_list;
static dso_info_t *dso_closed_list;

static dso_info_t *dso_free_list;



//*******************************************************************************
// forward declarations
//*******************************************************************************

static void        fnbounds_tmpdir_remove();
static int         fnbounds_tmpdir_create();
static char *      fnbounds_tmpdir_get();

static dso_info_t *fnbounds_dso_info_get(void *pc);
static dso_info_t *fnbounds_compute(const char *filename, void *start, void *end);
static dso_info_t *fnbounds_dso_info_query(void *pc, dso_info_t * dl_list);
static dso_info_t *fnbounds_dso_handle_open(const char *module_name, void *start, void *end);
static void   fnbounds_map_executable();

static dso_info_t *new_dso_info_t(const char *name, void **table, int nsymbols, int relocate, 
				  void *startaddr, void *endaddr, void *dl_handle);

static const char *mybasename(const char *string);

static dso_info_t *dso_list_head(dso_info_t *dso_list);
static void        dso_list_add(dso_info_t **dso_list, dso_info_t *self);
static void        dso_list_remove(dso_info_t **dso_list, dso_info_t *self);

static dso_info_t *dso_info_allocate();
static void        dso_info_free(dso_info_t *unused);

static char *nm_command = 0;

//*******************************************************************************
// interface operations  
//*******************************************************************************

//-------------------------------------------------------------------------------
// function fnbounds_init: 
// 
//     for dynamically-linked executables, start an fnbounds server process to 
//     that will compute function bounds information upon demand for 
//     dynamically-linked load modules.
//
//     return code = 0 upon success, otherwise fork failed 
//
//     NOTE: don't make this routine idempotent: it may be needed to start a 
//                 new server if the process forks
//-------------------------------------------------------------------------------
int 
fnbounds_init()
{
  int result = system_server_start();
  if (result == 0) {
    result = fnbounds_tmpdir_create();
    if (result == 0) {
      nm_command = getenv("CSPROF_NM_COMMAND"); 
  
      fnbounds_map_executable();
      fnbounds_map_open_dsos();
    } else {
      system_server_shutdown();
    }
  }
  return result;
}


int
fnbounds_enclosing_addr(void *pc, void **start, void **end)
{
  int ret = 1; // failure unless otherwise reset to 0 below

  dso_info_t *r = fnbounds_dso_info_get(pc);
  
  if (r && r->nsymbols > 0) { 
    void * relative_pc = pc;

    if (r->relocate) {
      relative_pc =  (void *) ((unsigned long) relative_pc) - 
	((unsigned long) r->start_addr); 
    }

    ret =  fnbounds_table_lookup(r->table, r->nsymbols, relative_pc, 
				 (void **) start, (void **) end);

    if (ret == 0 && r->relocate) {
      *start = PERFORM_RELOCATION(*start, r->start_addr);
      *end   = PERFORM_RELOCATION(*end  , r->start_addr);
    }
  }

  return ret;
}


//-------------------------------------------------------------------------------
// Function: fnbounds_map_open_dsos
// Purpose:  
//     identify any new dsos that have been mapped.
//     analyze them and add their information to the open list.
//-------------------------------------------------------------------------------
void
fnbounds_map_open_dsos()
{
  dylib_map_open_dsos();
}


//-------------------------------------------------------------------------------
// Function: fnbounds_unmap_closed_dsos
// Purpose:  
//     identify any dsos that are no longer mapped.
//     move them from the open to the closed list. 
//-------------------------------------------------------------------------------
void
fnbounds_unmap_closed_dsos()
{
  dso_info_t *dso_info, *next;
  for (dso_info = dso_open_list; dso_info; dso_info = next) {
    next = dso_info->next;
    if (!dylib_addr_is_mapped((unsigned long long) dso_info->start_addr)) {
      
      // remove from open list of DSOs
      dso_list_remove(&dso_open_list, dso_info);

      // add to closed list of DSOs 
      dso_list_add(&dso_closed_list, dso_info);

      monitor_real_dlclose(dso_info->handle);
    }
  }
}


int
fnbounds_note_module(const char *module_name, void *start, void *end)
{
  int success;

  //-----------------------------------------------------------------------------
  // check if the file is a dso containing fnbounds information that we mapped
  // by checking to see if the mapped file is in csprof's temporary directory.
  //-----------------------------------------------------------------------------
  if (strncmp(fnbounds_tmpdir, module_name, strlen(fnbounds_tmpdir)) == 0) {
    success = 1; // it is one of ours, no processing needed. indicate success.
  } else if (fnbounds_dso_info_query(start, dso_open_list)) {
    success = 1; // already mapped
  } else {
    dso_info_t *dso_info = fnbounds_dso_handle_open(module_name, start, end);
    success =  (dso_info ? 1 : 0); 
  } 

  return success;
}
  

int
fnbounds_module_domap(const char *incoming_filename, void *start, void *end)
{
  return (fnbounds_compute(incoming_filename, start, end) != NULL);
}


//-------------------------------------------------------------------------------
// function fnbounds_fini: 
// 
//     for dynamically-linked executables, shut down  the fnbounds server process 
//-------------------------------------------------------------------------------
void 
fnbounds_fini()
{
  system_server_shutdown();
  fnbounds_tmpdir_remove();
}



/******************************************************************************
 * private operations
 *****************************************************************************/

static dso_info_t *
fnbounds_compute(const char *incoming_filename, void *start, void *end)
{
  char filename[PATH_MAX];
  realpath(incoming_filename, filename);

  if (nm_command) {
    char command[MAXPATHLEN+1024];
    char dlname[MAXPATHLEN];
    char redir[64] = {'\0'};

    //    IF_NOT_DISABLED(DL_BOUND_REDIR) {
    // }

    int logfile_fd = csprof_logfile_fd();
    sprintf(redir,"1>&%d 2>&%d",logfile_fd, logfile_fd);

    sprintf(dlname, "%s/%s.nm.so", fnbounds_tmpdir_get(), mybasename(filename));
    char *script_debug = "";
    IF_ENABLED(DL_BOUND_SCRIPT_DEBUG) {
      script_debug = "DBG";
    }

    sprintf(command, "%s %s %s %s %s\n", nm_command, filename, 
            fnbounds_tmpdir_get(), script_debug, redir);
    TMSG(DL_BOUND,"system command = %s",command);

    int result = system_server_execute_command(command);
    if (result) {
      EMSG("fnbounds server command failed for file %s, aborting", filename);
      abort();
    } else {
      int nsymbols = 0; // default is that there are no symbols
      int relocate = 0; // default is no relocation
      int addmapping = 0; // default is omit
      void *dlhandle = monitor_real_dlopen(dlname, RTLD_LAZY | RTLD_LOCAL);
      int *bogus = dlsym(dlhandle,"BOGUS");
      if (bogus){
	EMSG("fnbounds computed bogus symbols for file %s, aborting",filename);
	abort();
      }
      void **nm_table = dlsym(dlhandle, "csprof_nm_addrs");
      int *nm_table_len = (int *) dlsym(dlhandle, "csprof_nm_addrs_len");
      int *is_relocatable = (int *) dlsym(dlhandle, "csprof_relocatable");
      int *stripped = (int *) dlsym(dlhandle, "csprof_stripped");
      if (stripped && *stripped == 0 && nm_table_len && is_relocatable) {
	// file is not stripped. the number of symbols and relocation 
	// information are available. in this case, we can consider using
	// symbols values available in the table.
	nsymbols = *nm_table_len;
        relocate = *is_relocatable;
	if (nsymbols >= 1) {
	  if (relocate) {
	    if (nm_table[0] >= start && nm_table[0] <= end)
	       relocate = 0; // segment loaded at its preferred address
          } else {
             char executable_name[PATH_MAX];
	     unsigned long long mstart, mend;
             if (dylib_find_module_containing_addr(
                     (unsigned long long) nm_table[0], 
		     executable_name, &mstart, &mend)) {
                start = (void *) mstart;
                end = (void *) mend;
             } else {
                start = nm_table[0];
                end = nm_table[nsymbols - 1];
             } 
          }
          addmapping = 1;
	}
      }
      if (addmapping) {
	dso_info_t *r = new_dso_info_t(filename, nm_table, nsymbols, relocate, 
                                       start, end, dlhandle);
         return r;
      }
    }
  }
  return 0;
}


void 
fnbounds_epoch_finalize()
{
  dso_info_t * dso_info;
  for (dso_info = dso_open_list; dso_info; dso_info = dso_info->next) {
    csprof_epoch_add_module(dso_info->name, NULL /* no vaddr */, dso_info->start_addr, 
	dso_info->end_addr - dso_info->start_addr);
  } 

  dso_info_t *next;
  for (dso_info = dso_closed_list; dso_info;) {
    csprof_epoch_add_module(dso_info->name, NULL /* no vaddr */, dso_info->start_addr, 
	dso_info->end_addr - dso_info->start_addr);
    next = dso_info->next;
    dso_list_remove(&dso_closed_list, dso_info);
    dso_info_free(dso_info);
    dso_info = next;
  } 
}


static dso_info_t *
fnbounds_dso_info_query(void *pc, dso_info_t * dl_list)
{
  dso_info_t *dso_info = dl_list;

  //-----------------------------------------------------------------------------
  // see if we already have function bounds information computed for a dso 
  // containing this pc
  //-----------------------------------------------------------------------------
  while (dso_info && (pc < dso_info->start_addr || pc > dso_info->end_addr)) 
    dso_info = dso_info->next; 

  return dso_info;
}


static dso_info_t *
fnbounds_dso_handle_open(const char *module_name, void *start, void *end)
{
  dso_info_t *dso_info = fnbounds_dso_info_query(start, dso_closed_list);
  // the address range of the module, which does not have an open mapping
  // was found to conflict with the address range of a module on the closed
  // list. 
  if (dso_info) {
    if (strcmp(module_name, dso_info->name) == 0 && 
        start == dso_info->start_addr && 
        end == dso_info->end_addr) {
      // reopening a closed module at the same spot in the address. 
      // move the record from the closed list to the open list, 
      // reopen the symbols, and we are done.

      // remove from closed list of DSOs
      dso_list_remove(&dso_closed_list, dso_info);

      // place dso_info on the free list. it will immediately be reclaimed by 
      // fnbounds_compute which will fill in the data and remap the fnbounds 
      // information into memory.
      dso_info_free(dso_info);

      // NOTE: if we refactored fnbounds_compute, we could avoid some of the 
      // costs since bounds information must already be computed on disk and 
      // only needs to be mapped. however, this doesn't seem worth the effort 
      // at present.
    } else {
      // the entry on the closed list was not the same module
      fnbounds_epoch_finalize();
      csprof_epoch_new();
    }
  }
  dso_info = fnbounds_compute(module_name, start, end);
  return dso_info;
}


static dso_info_t *
fnbounds_dso_info_get(void *pc)
{
  dso_info_t *dso_open = fnbounds_dso_info_query(pc, dso_open_list);

  if (!dso_open) {
    //--------------------------------------------------------------------------
    // we don't have any function bounds information for an open dso 
    // containing this pc.
    //--------------------------------------------------------------------------

    if (csprof_dlopen_pending()) {
      //------------------------------------------------------------------------
      // a new dso might have been just mapped without our knowledge.
      // see if we can locate the name and address range of a dso 
      // containing this pc 
      //------------------------------------------------------------------------
      char module_name[PATH_MAX];
      unsigned long long addr, mstart, mend;
      addr = (unsigned long long) pc;
      
      if (dylib_find_module_containing_addr(addr, module_name, &mstart, &mend)) {
	dso_open = fnbounds_dso_handle_open(module_name, (void *) mstart, (void *) mend);
      }
    }
  }

  return dso_open;
}


static void
fnbounds_map_executable()
{
  dylib_map_executable();
}


static dso_info_t *
new_dso_info_t(const char *name, void **table, int nsymbols, int relocate, 
	       void *startaddr, void *endaddr, void *dl_handle) 
{
  int namelen = strlen(name) + 1;
  dso_info_t *r  = dso_info_allocate();
  
  r->name = (char *) csprof_malloc(namelen);
  strcpy(r->name, name);

  r->table = table;
  r->nsymbols = nsymbols;

  r->relocate = relocate;
  r->start_addr = startaddr;
  r->end_addr = endaddr;

  r->handle = dl_handle;

  dso_list_add(&dso_open_list, r);

  return r;
}


static const char *
mybasename(const char *string)
{
  char *suffix = rindex(string, '/');
  if (suffix) return suffix + 1;
  else return string;
}


/******************************************************************************
 * temporary directory
 *****************************************************************************/

static int 
fnbounds_tmpdir_create()
{
  int i, result;
  // try multiple times to create a temporary directory 
  // with the aim of avoiding failure
  for (i = 0; i < 10; i++) {
    sprintf(fnbounds_tmpdir,"%s/%d-%d", tmproot, (int) getpid(),i);
    result = mkdir(fnbounds_tmpdir, 0777);
    if (result == 0) break;
  }
  if (result)  {
    char buffer[1024];
    EMSG("fatal error: unable to make temporary directory %s (error = %s)\n", 
          fnbounds_tmpdir, strerror_r(errno, buffer, 1024));
  } 
  return result;
}


static char *
fnbounds_tmpdir_get()
{
  return fnbounds_tmpdir;
}


static void 
fnbounds_tmpdir_remove()
{
  IF_NOT_DISABLED(DL_BOUND_UNLINK){
    unlink_tree(fnbounds_tmpdir);
  }
}


/******************************************************************************
 * list operations 
 *****************************************************************************/

static dso_info_t * 
dso_list_head(dso_info_t *dso_list)
{
  return dso_list;
}

static void 
dso_list_add(dso_info_t **dso_list, dso_info_t *self)
{
  // add self at head of list
  self->next = *dso_list;
  self->prev = NULL;
  *dso_list = self;
}

static void 
dso_list_remove(dso_info_t **dso_list, dso_info_t *self)
{
  dso_info_t *prev = self->prev;

  if (prev) {
    // have a predecessor: not at head of list 
    // 1. adjust predecessor to point to my successor 
    // 2. forget about my predecessor 
    prev->next = self->next;
    self->prev = NULL;
  } else {
    // no predecessor: at head of list
    // repoint list head to next 
    *dso_list = self->next;
  }

  // forget about my successor 
  self->next = 0;
}


/******************************************************************************
 * allocation/deallocation of dso_info_t records
 *****************************************************************************/

static dso_info_t *
dso_info_allocate()
{
  static dso_info_t *dso_free_list = 0;
  dso_info_t *new = dso_list_head(dso_free_list);
  if (new) {
    dso_list_remove(&dso_free_list, new);
  } else {
    new = (dso_info_t *) csprof_malloc(sizeof(dso_info_t));
  }
  return new;
}


static void
dso_info_free(dso_info_t *unused)
{
  dso_list_add(&dso_free_list, unused);
}


/******************************************************************************
 * debugging support
 *****************************************************************************/

void 
dump_dso_info_t(dso_info_t *r)
{
  printf("%p-%p %s [dso_info_t *%p, table=%p, nsymbols=%d, relocatable=%d]\n", r->start_addr, r->end_addr, r->name, 
         r, r->table, r->nsymbols, r->relocate);
#if 0
  printf("record addr = %p: name = '%s', table = %p, nsymbols = 0x%x, "
	 "start = %p, end = %p, relocate = %d\n",
         r, r->name, r->table, r->nsymbols, r->start_addr, 
	 r->end_addr, r->relocate);
#endif
}

void 
dump_dso_list(dso_info_t *dl_list)
{
  dso_info_t *r = dl_list;
  for (; r; r = r->next) dump_dso_info_t(r);
}
