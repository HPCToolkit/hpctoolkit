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

// This file opens the three types of files that hpcrun uses: .log,
// .hpcrun and .hpctrace.  The division of labor is that files.c knows
// about file names, opens the file and returns a file descriptor.
// Everything else just uses the fd.
//
// Major technical problems: (1) we name files by MPI rank, but we
// open the log and trace files before we know their rank.  Thus, we
// have to open them early and rename them later.  (2) we need a
// unique id for all files from the same process.  (3) on some
// systems, gethostid does not return a unique value across all nodes.
// So, (hostid, pid) is not always unique to the process.  Plus, a
// process can exec the same binary (same pid).
//
// The solution is to use rank, threadid, hostid, pid and a generation
// number to make unique names.
//
//   progname-rank-thread-hostid-pid-gen.suffix
//
// Normally, (hostid, pid) uniquely identifies the process, but not
// always.  We open() the file with O_EXCL.  If that succeeds, then we
// win the race and (hostid, pid, gen) is the unique id for this
// process.  If not, then bump the gen number and try again.
//
// If this continues to fail for several revs, then start picking
// random hostids.  Possibly, a process could be exec-ing itself
// several times, but more likely either gethostid() or getpid() is
// not returning unique values on this system.
//
// Also, since we rename a file from rank 0 to its actual rank, we use
// both an early and late id.
//
// Note: we use the .log file as the lock for all file names for this
// process, for both early and late ids.  That is, the process that
// opens the file "prog-0-0-host-pid-gen.log" owns all file names of
// the form "prog-0-*-host-pid-gen.*" (early id).  Also, the process
// that opens "prog-rank-0-host-pid-gen.log" owns all file names of
// the form "prog-rank-*-host-pid-gen.*" (late id).
//
// Thus, it is necessary to open the .log file first and also rename
// the .log file as the first late name.
//
// Note: it is important to open() the files with O_EXCL to determine
// the winner of the lock.  Also, when renaming a file, it's important
// to link() the new name and then unlink() the old name if successful
// instead of using rename().  link(2) returns EEXIST if the new name
// already exists, whereas rename(2) silently overwrites an existing
// file.
//
// Note: it's ok to rename a file with an open file descriptor.
//
// It would make sense to replace the (hostid, pid, gen) ids with a
// single random number of some length, again testing with O_EXCL and
// using a different value if necessary.


//***************************************************************
// global includes 
//***************************************************************

#include <errno.h>   // errno
#include <fcntl.h>   // open
#include <limits.h>  // PATH_MAX
#include <stdio.h>   // sprintf
#include <stdlib.h>  // realpath
#include <string.h>  // strerror
#include <unistd.h>  // gethostid
#include <sys/time.h>   // gettimeofday
#include <sys/types.h>  // struct stat
#include <sys/stat.h>   // stat 
#include <stdbool.h>


//***************************************************************
// local includes 
//***************************************************************

#include "env.h"
#include "disabled.h"
#include "files.h"
#include "messages.h"
#include "thread_data.h"
#include "loadmap.h"
#include "sample_prob.h"

#include <lib/prof-lean/spinlock.h>
#include <lib/support-lean/OSUtil.h>


//***************************************************************
// macros
//***************************************************************

// directory/progname-rank-thread-hostid-pid-gen.suffix
#define FILENAME_TEMPLATE  "%s/%s-%06u-%03d-%08lx-%u-%d.%s"

#define FILES_RANDOM_GEN  4
#define FILES_MAX_GEN     11

#define FILES_EARLY  0x1
#define FILES_LATE   0x2

struct fileid {
  int  done;
  long host;
  int  gen;
};


//***************************************************************
// globals 
//***************************************************************



//***************************************************************
// forward declarations 
//***************************************************************

static void hpcrun_rename_log_file_early(int rank);


//***************************************************************
// local data 
//***************************************************************

static char default_path[PATH_MAX] = {'\0'};
static char output_directory[PATH_MAX] = {'\0'};
static char executable_name[PATH_MAX] = {'\0'};
static char executable_pathname[PATH_MAX] = {'\0'};

// These variables are protected by the files lock.
// Opening or renaming a file must acquire the lock.

static spinlock_t files_lock = SPINLOCK_UNLOCKED;
static pid_t mypid = 0;
static struct fileid earlyid;
static struct fileid lateid;
static int log_done = 0;
static int log_rename_done = 0;
static int log_rename_ret = 0;


//***************************************************************
// private operations
//***************************************************************

// In general, the external functions acquire the files lock and the
// internal functions require that the lock is already held.

// Reset the file ids on first use (pid 0) or after fork.
static void
hpcrun_files_init(void)
{
  pid_t cur_pid = getpid();

  if (mypid != cur_pid) {
    mypid = cur_pid;
    earlyid.done = 0;
    earlyid.host = OSUtil_hostid();
    earlyid.gen = 0;
    lateid = earlyid;
    log_done = 0;
    log_rename_done = 0;
    log_rename_ret = 0;
  }
}


// Replace "id" with the next unique id if possible.  Normally,
// (hostid, pid, gen) works after one or two iterations.  To be extra
// robust (eg, hostid is not unique), at some point, give up and pick
// a random hostid.
//
// Returns: 0 on success, else -1 on failure.
//
static int
hpcrun_files_next_id(struct fileid *id)
{
  struct timeval tv;
  int fd;

  if (id->done || id->gen >= FILES_MAX_GEN) {
    // failure, out of options
    return -1;
  }

  id->gen++;
  if (id->gen >= FILES_RANDOM_GEN) {
    // give up and use a random host id
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
      read(fd, &id->host, sizeof(id->host));
      close(fd);
    }
    gettimeofday(&tv, NULL);
    id->host += (tv.tv_sec << 20) + tv.tv_usec;
    id->host &= 0x00ffffffff;
  }

  return 0;
}


// Open the file with O_EXCL and try the next file id if it already
// exists.  The log and trace files are opened early, the profile file
// (hpcrun) is opened late.  Must hold the files lock.

// Returns: file descriptor, else die on failure.
//
static int
hpcrun_open_file(int rank, int thread, const char *suffix, int flags)
{
  char name[PATH_MAX];
  struct fileid *id;
  int fd, ret;

  // If not recording data for this process, then open /dev/null.
  if (! hpcrun_sample_prob_active()) {
    fd = open("/dev/null", O_WRONLY);
    return fd;
  }

  id = (flags & FILES_EARLY) ? &earlyid : &lateid;
  for (;;) {
    errno = 0;
    ret = snprintf(name, PATH_MAX, FILENAME_TEMPLATE, output_directory,
		   executable_name, rank, thread, id->host, mypid, id->gen, suffix);
    if (ret >= PATH_MAX) {
      fd = -1;
      errno = ENAMETOOLONG;
      break;
    }
    fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd >= 0) {
      // success
      break;
    }
    if (errno != EEXIST || hpcrun_files_next_id(id) != 0) {
      // failure, out of options
      fd = -1;
      break;
    }
  }
  id->done = 1;
  if (flags & FILES_EARLY) {
    // late id starts where early id is chosen
    lateid = earlyid;
    lateid.done = 0;
  }

  // Failure to open is a fatal error.
  if (fd < 0) {
    hpcrun_abort("hpctoolkit: unable to open %s file: '%s': %s",
		 suffix, name, strerror(errno));
  }

  return fd;
}


// Rename the file from MPI rank 0 and early id to new rank and late
// id (rename is always late).  Must hold the files lock.
//
// Note: must use link(2) and unlink(2) instead of rename(2) to
// atomically test if the new file exists.  rename() silently
// overwrites a previous file.
//
// Returns: 0 on success, else -1 on failure.
static int
hpcrun_rename_file(int rank, int thread, const char *suffix)
{
  char old_name[PATH_MAX], new_name[PATH_MAX];
  int ret;

  // Not recoding data for this process.
  if (! hpcrun_sample_prob_active()) {
    return 0;
  }

  // Old and new names are the same.
  if (rank == 0 && earlyid.host == lateid.host && earlyid.gen == lateid.gen) {
    return 0;
  }

  snprintf(old_name, PATH_MAX, FILENAME_TEMPLATE, output_directory,
	   executable_name, 0, thread, earlyid.host, mypid, earlyid.gen, suffix);
  for (;;) {
    errno = 0;
    ret = snprintf(new_name, PATH_MAX, FILENAME_TEMPLATE, output_directory,
		   executable_name, rank, thread, lateid.host, mypid, lateid.gen, suffix);
    if (ret >= PATH_MAX) {
      ret = -1;
      errno = ENAMETOOLONG;
      break;
    }
    ret = link(old_name, new_name);
    if (ret == 0) {
      // success
      unlink(old_name);
      break;
    }
    if (errno != EEXIST || hpcrun_files_next_id(&lateid) != 0) {
      // failure, out of options
      ret = -1;
      break;
    }
  }
  lateid.done = 1;

  // Failure to rename is a loud warning.
  if (ret < 0) {
    EEMSG("hpctoolkit: unable to rename %s file: '%s' -> '%s': %s",
	  suffix, old_name, new_name, strerror(errno));
    EMSG("hpctoolkit: unable to rename %s file: '%s' -> '%s': %s",
	 suffix, old_name, new_name, strerror(errno));
  }

  return ret;
}


//***************************************************************
// interface operations
//***************************************************************

const char*
hpcrun_files_executable_pathname()
{
  const char* load_name = hpcrun_loadmap_findLoadName(executable_name);

  return load_name ? load_name : executable_name;
}


const char * 
hpcrun_files_executable_name()
{
  return executable_name;
}


void 
hpcrun_files_set_directory()
{  
  char *path = getenv(HPCRUN_OUT_PATH);

  // compute path for default measurement directory
  if (path == NULL || strlen(path) == 0) {
    const char *jid = OSUtil_jobid();
    if (jid == NULL) {
      sprintf(default_path, "./hpctoolkit-%s-measurements", executable_name);
    } else {
      sprintf(default_path, "./hpctoolkit-%s-measurements-%s", executable_name, jid);
    }
    path = default_path;
    // N.B.: safe to skip checking for errors as realpath will notice them
  }

  int ret = mkdir(path, 0755);
  if (ret != 0 && errno != EEXIST) {
    hpcrun_abort("hpcrun: could not create output directory `%s': %s",
		 path, strerror(errno));
  }

  char* rpath = realpath(path, output_directory);
  if (!rpath) {
    hpcrun_abort("hpcrun: could not access directory `%s': %s", path, strerror(errno));
  }
}


void 
hpcrun_files_set_executable(char *execname)
{
  strncpy(executable_name, basename(execname), sizeof(executable_name));

  if ( ! realpath(execname, executable_pathname) ) {
    strncpy(executable_pathname, execname, sizeof(executable_pathname));
  }
}


// Returns: file descriptor for log file.
int
hpcrun_open_log_file(void)
{
  int ret;

  spinlock_lock(&files_lock);
  hpcrun_files_init();
  ret = hpcrun_open_file(0, 0, HPCRUN_LogFnmSfx, FILES_EARLY);
  if (ret >= 0) {
    log_done = 1;
  }
  spinlock_unlock(&files_lock);

  return ret;
}


// Returns: file descriptor for trace file.
int
hpcrun_open_trace_file(int thread)
{
  int ret;

  TMSG(TRACE, "Opening trace file for %d", thread);
  spinlock_lock(&files_lock);
  TMSG(TRACE, "Calling files init for %d", thread);
  hpcrun_files_init();
  TMSG(TRACE, "About to open file for %d", thread);
  ret = hpcrun_open_file(0, thread, HPCRUN_TraceFnmSfx, FILES_EARLY);
  TMSG(TRACE, "Back from open file %d, ret code = %d", thread, ret);
  spinlock_unlock(&files_lock);
  TMSG(TRACE, "Unlocked file lock for %d", thread);

  return ret;
}

// Returns: file descriptor for profile (hpcrun) file.
int
hpcrun_open_profile_file(int rank, int thread)
{
  int ret;

  spinlock_lock(&files_lock);
  hpcrun_files_init();
  hpcrun_rename_log_file_early(rank);
  ret = hpcrun_open_file(rank, thread, HPCRUN_ProfileFnmSfx, FILES_LATE);
  spinlock_unlock(&files_lock);

  return ret;
}


// Note: we use the log file as the lock for the file names, so we
// need to rename the log file as the first late action.  Since this
// is out of sequence, we save the return value and return it when the
// log rename is actually called.  Must hold the files lock.
//
static void
hpcrun_rename_log_file_early(int rank)
{
  if (log_done && !log_rename_done) {
    log_rename_ret = hpcrun_rename_file(rank, 0, HPCRUN_LogFnmSfx);
    log_rename_done = 1;
  }
}


// Returns: 0 on success, else -1 on failure.
int
hpcrun_rename_log_file(int rank)
{
  spinlock_lock(&files_lock);
  hpcrun_rename_log_file_early(rank);
  spinlock_unlock(&files_lock);

  return log_rename_ret;
}


// Returns: 0 on success, else -1 on failure.
int
hpcrun_rename_trace_file(int rank, int thread)
{
  int ret;

  TMSG(TRACE, "Renaming trace file for rank %d, thread %d", rank, thread);
  spinlock_lock(&files_lock);
  TMSG(TRACE, "(Rename) Spin lock acquired for (R:%d, T:%d)", rank, thread);
  hpcrun_rename_log_file_early(rank);
  TMSG(TRACE, "Rename log file early (R:%d, T:%d)", rank, thread);
  ret = hpcrun_rename_file(rank, thread, HPCRUN_TraceFnmSfx);
  TMSG(TRACE, "Back from rename trace file for(R:%d, T:%d), retcode = %d", rank, thread, ret);
  spinlock_unlock(&files_lock);
  TMSG(TRACE, "(rename) Spin lock released for (R:%d, T:%d)", rank, thread);

  return ret;
}
