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

//***************************************************************
// global includes 
//***************************************************************

#include <errno.h>   // errno
#include <limits.h>  // PATH_MAX
#include <stdio.h>   // sprintf
#include <stdlib.h>  // realpath
#include <string.h>  // strerror
#include <unistd.h>  // gethostid
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



//***************************************************************
// macros
//***************************************************************

#define NO_HOST_ID      (-1)



//***************************************************************
// globals 
//***************************************************************



//***************************************************************
// forward declarations 
//***************************************************************

static char *os_realpath(const char *inpath, char *outpath);

static char *files_name(char *filename, unsigned int mpi, const char *suffix);


//***************************************************************
// local data 
//***************************************************************

static char default_path[PATH_MAX] = {'\0'};
static char output_directory[PATH_MAX] = {'\0'};
static char executable_name[PATH_MAX] = {'\0'};
static char executable_pathname[PATH_MAX] = {'\0'};

//***************************************************************
// interface operations
//***************************************************************

void
files_trace_name(char *filename, unsigned int mpi, int len)
{
  files_name(filename, mpi, HPCRUN_TraceFnmSfx);
}


const char*
files_executable_pathname()
{
  const char* load_name = hpcrun_loadmap_findLoadName(executable_name);

  return load_name ? load_name : executable_name;
}

const char * 
files_executable_name()
{
  return executable_name;
}


void
files_profile_name(char *filename, unsigned int mpi, int len)
{
  files_name(filename, mpi, HPCRUN_ProfileFnmSfx);
}


void
files_log_name(char *filename, unsigned int mpi, int len)
{
  files_name(filename, mpi, HPCRUN_LogFnmSfx);
}


void 
files_set_directory()
{  
  char *path = getenv(HPCRUN_OPT_OUT_PATH);

  // compute path for default measurement directory
  if (path == NULL || strlen(path) == 0) {
    const char *jid = os_job_id();
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
    hpcrun_abort("hpcrun: could not create output directory `%s': %s", path, strerror(errno));
  }

  char* rpath = os_realpath(path, output_directory);
  if (!rpath) {
    hpcrun_abort("hpcrun: could not access directory `%s': %s", path, strerror(errno));
  }
}


void 
files_set_executable(char *execname)
{
  strncpy(executable_name, basename(execname), sizeof(executable_name));

  if ( ! realpath(execname, executable_pathname) ) {
    strncpy(executable_pathname, execname, sizeof(executable_pathname));
  }
}

//*****************************************************************************

long
os_hostid()
{
  static long hostid = NO_HOST_ID;

  if (hostid == NO_HOST_ID) {
    // gethostid returns a 32-bit id.  treat it as unsigned 
    // to prevent useless sign extension
    hostid = (uint32_t) gethostid();
  }

  return hostid;
}


unsigned int
os_pid()
{
  return getpid();
}

const char* 
os_job_id()
{
  char *jid = NULL;
  if (jid == NULL) {
    jid = getenv("COBALT_JOBID"); /* check for Cobalt job id */
  }
  if (jid == NULL) {
    jid = getenv("PBS_JOBID"); /* check for PBS job id */
  }
  if (jid == NULL) {
    jid = getenv("JOB_ID"); /* check for Sun Grid Engine job id */
  }
  return jid;
}


//***************************************************************
// private operations
//***************************************************************

static char *
os_realpath(const char *inpath, char *outpath)
{
   return realpath(inpath, outpath);
}


static char *
files_name(char* filename, unsigned int mpi, const char* suffix)
{
  thread_data_t *td = hpcrun_get_thread_data();

  sprintf(filename, "%s/%s-%06u-%03d-%lx-%u.%s",
          output_directory, executable_name, mpi,
          td->id,
          os_hostid(), os_pid(), suffix); 

  return filename;
}
