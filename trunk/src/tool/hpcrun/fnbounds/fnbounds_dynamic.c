// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <dlfcn.h>     // for dlopen/dlclose
#include <string.h>    // for strcmp, strerror
#include <stdlib.h>    // for getenv
#include <errno.h>     // for errno
#include <sys/mman.h>
#include <sys/param.h> // for PATH_MAX
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>    // getpid
#include <fcntl.h>

//*********************************************************************
// external libraries
//*********************************************************************

#include <monitor.h>

//*********************************************************************
// local includes
//*********************************************************************

#include "fnbounds_interface.h"

#include "dylib.h"

#include <hpcrun_dlfns.h>
#include <hpcrun_stats.h>
#include <disabled.h>
#include <loadmap.h>
#include <files.h>
#include <epoch.h>
#include <sample_event.h>
#include <system_server.h>
#include <thread_data.h>

#include <utilities/unlink.h>

#include <unwind/common/ui_tree.h>

#include <messages/messages.h>

// FIXME:tallent: more spaghetti includes
#include <hpcfnbounds/fnbounds-file-header.h>

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

// FIXME: tmproot should be overridable with an option.
static char *tmproot = "/tmp";

static char fnbounds_tmpdir[PATH_MAX];

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

static void        fnbounds_tmpdir_remove();
static int         fnbounds_tmpdir_create();
static char *      fnbounds_tmpdir_get();

static dso_info_t *fnbounds_dso_info_get(void *pc);
static dso_info_t *fnbounds_compute(const char *filename,
				    void *start, void *end);

static void        fnbounds_map_executable();


static const char *mybasename(const char *string);

static char *nm_command = 0;


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
  FNBOUNDS_LOCK;

  int ret = 1; // failure unless otherwise reset to 0 below

  dso_info_t *r = fnbounds_dso_info_get(pc);
  
  if (r && r->nsymbols > 0) { 
    void * relative_pc = pc;
    if (r->relocate) {
      relative_pc =  
	(void *) (((unsigned long) relative_pc) - r->start_to_ref_dist);
    }

    ret =  fnbounds_table_lookup(r->table, r->nsymbols, relative_pc, 
				 (void **) start, (void **) end);

    if (ret == 0 && r->relocate) {
      *start = PERFORM_RELOCATION(*start, r->start_to_ref_dist);
      *end   = PERFORM_RELOCATION(*end  , r->start_to_ref_dist);
    }
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


int
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

  load_module_t *current = hpcrun_getLoadmap()->lm_head;
  while (current && current->dso_info) {
    if (!dylib_addr_is_mapped(current->dso_info->start_addr)) {
      hpcrun_loadmap_unmap(current);
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

  system_server_shutdown();
  fnbounds_tmpdir_remove();
}


void
fnbounds_release_lock(void)
{
  FNBOUNDS_UNLOCK;  
}


//*********************************************************************
// private operations
//
// Note: the private operations should all assume that fnbounds_lock
// is already locked (mostly).
//*********************************************************************

//
// Read the binary file of function addresses from hpcfnbounds-bin and
// load into memory as an array of void *.
//
// Returns: pointer to array on success and fills in map_size and file
//          header, else NULL on failure.
//
static void *
fnbounds_read_nm_file(const char *file, long *map_size,
		      struct fnbounds_file_header *fh)
{
  struct stat st;
  char *table;
  long pagesize, len, ret;
  int fd;

  if (file == NULL || map_size == NULL || fh == NULL) {
    EMSG("passed NULL to fnbounds_read_nm_file");
    return (NULL);
  }
  if (stat(file, &st) != 0) {
    EMSG("stat failed on fnbounds file: %s", file);
    return (NULL);
  }
  if (st.st_size < sizeof(*fh)) {
    EMSG("fnbounds file too small (%ld bytes): %s",
	 (long)st.st_size, file);
    return (NULL);
  }
  //
  // Round up map_size to multiple of page size and mmap().
  //
  pagesize = getpagesize();
  *map_size = (st.st_size/pagesize + 1)*pagesize;
  table = mmap(NULL, *map_size, PROT_READ | PROT_WRITE,
	       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (table == NULL) {
    EMSG("mmap failed on fnbounds file: %s", file);
    return (NULL);
  }
  //
  // Read the file into memory.  Note: we read() the file instead of
  // mmap()ing it directly, so we can close() it immediately.
  //
  fd = open(file, O_RDONLY);
  if (fd < 0) {
    EMSG("open failed on fnbounds file: %s", file);
    return (NULL);
  }
  len = 0;
  while (len < st.st_size) {
    ret = read(fd, table + len, st.st_size - len);
    if (ret <= 0) {
      EMSG("read failed on fnbounds file: %s", file);
      close (fd);
      return (NULL);
    }
    len += ret;
  }
  close(fd);

  memcpy(fh, table + st.st_size - sizeof(*fh), sizeof(*fh));
  if (fh->magic != FNBOUNDS_MAGIC) {
    EMSG("bad magic in fnbounds file: %s", file);
    return (NULL);
  }
  if (st.st_size < fh->num_entries * sizeof(void *)) {
    EMSG("fnbounds file too small (%ld bytes, %ld entries): %s",
	 (long)st.st_size, (long)fh->num_entries, file);
    return (NULL);
  }
  return (void *)table;
}


static dso_info_t *
fnbounds_compute(const char *incoming_filename, void *start, void *end)
{
  char filename[PATH_MAX];
  char command[MAXPATHLEN+1024];
  char dlname[MAXPATHLEN];
  int  logfile_fd = messages_logfile_fd();

  if (nm_command == NULL || incoming_filename == NULL)
    return (NULL);

  realpath(incoming_filename, filename);
  sprintf(dlname, FNBOUNDS_BINARY_FORMAT, fnbounds_tmpdir_get(), 
	  mybasename(filename));

  sprintf(command, "%s -b %s %s %s 1>&%d 2>&%d\n",
	  nm_command, ENABLED(DL_BOUND_SCRIPT_DEBUG) ? "-t -v" : "",
	  filename, fnbounds_tmpdir_get(), logfile_fd, logfile_fd);
  TMSG(DL_BOUND, "system command = %s", command);

  int result = system_server_execute_command(command);
  if (result) {
    EMSG("fnbounds server command failed for file %s, aborting", filename);
    monitor_real_exit(1);
  }

  long map_size = 0;
  struct fnbounds_file_header fh;
  void **nm_table = (void **)fnbounds_read_nm_file(dlname, &map_size, &fh);
  if (nm_table == NULL) {
    EMSG("fnbounds computed bogus symbols for file %s, aborting",filename);
    monitor_real_exit(1);
  }

  if (fh.num_entries < 1)
    return (NULL);

  //
  // Note: we no longer care if binary is stripped.
  //
  if (fh.relocatable) {
    if (nm_table[0] >= start && nm_table[0] <= end) {
      // segment loaded at its preferred address
      fh.relocatable = 0;
    }
  } else {
    char executable_name[PATH_MAX];
    void *mstart; 
    void *mend;
    if (dylib_find_module_containing_addr(nm_table[0],
					  executable_name, &mstart, &mend)) {
      start = (void *) mstart;
      end = (void *) mend;
    } else {
      start = nm_table[0];
      end = nm_table[fh.num_entries - 1];
    }
  }
  
  dso_info_t *dso =
    hpcrun_dso_make(filename, nm_table, &fh, start, end, map_size);

  return dso;
}


static dso_info_t *
fnbounds_dso_info_get(void *pc)
{
  load_module_t* lm = hpcrun_loadmap_findByAddr(pc, pc);
  dso_info_t* dso_open = (lm) ? lm->dso_info : NULL;
  // We can't call dl_iterate_phdr() in general because catching a
  // sample at just the wrong point inside dlopen() will segfault or
  // deadlock.
  //
  // However, the risk is small, and if we're willing to take the
  // risk, then analyzing the new DSO here allows us to sample inside
  // an init constructor.
  //
  if (!dso_open && ENABLED(DLOPEN_RISKY) && hpcrun_dlopen_pending() > 0) {
    char module_name[PATH_MAX];
    void *mstart, *mend;
    
    if (dylib_find_module_containing_addr(pc, module_name, &mstart, &mend)) {
      dso_open = fnbounds_compute(module_name, mstart, mend);
      if (dso_open) {
	hpcrun_loadmap_map(dso_open);
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


static const char *
mybasename(const char *string)
{
  char *suffix = rindex(string, '/');
  if (suffix) return suffix + 1;
  else return string;
}


//*********************************************************************
// temporary directory
//*********************************************************************

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
  if (! ENABLED (DL_BOUND_RETAIN_TMP)){
    unlink_tree(fnbounds_tmpdir);
  }
}


