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



//***************************************************************
// local includes 
//***************************************************************

#include "env.h"
#include "files.h"
#include "pmsg.h"
#include "thread_data.h"



//***************************************************************
// macros
//***************************************************************

#define NO_HOST_ID      (-1)
#define NO_PID          (~0)



//***************************************************************
// forward declarations 
//***************************************************************

static char *files_name(char *filename, unsigned int mpi, const char *suffix);

static unsigned int os_pid();
static long os_hostid();
static char *os_realpath(const char *inpath, char *outpath);



//***************************************************************
// local data 
//***************************************************************

static char default_path[PATH_MAX];
static char output_directory[PATH_MAX];
static char *executable_name = 0;
static char *executable_pathname = 0;


//***************************************************************
// interface operations
//***************************************************************

void 
files_trace_name(char *filename, unsigned int mpi, int len)
{
  files_name(filename, mpi, CSPROF_TRACE_FNM_SFX);
}


const char *
files_executable_pathname(void)
{
  return executable_pathname;
}


const char * 
files_executable_name()
{
  return executable_name;
}


void
files_profile_name(char *filename, unsigned int mpi, int len)
{
  files_name(filename, mpi, CSPROF_PROFILE_FNM_SFX);
}


void
files_log_name(char *filename, int len)
{
  sprintf(filename, "%s/%s-%lx-%u.%s", 
	  output_directory, executable_name, os_hostid(), 
	  os_pid(), CSPROF_LOG_FNM_SFX);
}

void files_set_job_id() 
{
}


void 
files_set_directory()
{  
  char *path = getenv(CSPROF_OPT_OUT_PATH);

  if (path == NULL || strlen(path) == 0) {
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
    if (jid == NULL) {
      sprintf(default_path,"./hpctoolkit-%s-measurements", executable_name);
    } else {
      sprintf(default_path,"./hpctoolkit-%s-measurements-%s", executable_name, jid);
    }
    path = default_path;
    mkdir(default_path, 0755);
    // N.B.: safe to skip checking for errors as realpath will notice them
  }

  int dir_error = 0;
  if (os_realpath(path, output_directory) == NULL) dir_error = 1;
  else {
    struct stat sbuf;
    int sresult = stat(output_directory, &sbuf);
    if ((sresult != 0) || (S_ISDIR(sbuf.st_rdev) != 0)) dir_error = 1;
  }

  if (dir_error) csprof_abort("hpcrun: could not access path `%s': %s", path, strerror(errno));
}


static const size_t PATH_MAX_LEN = 2048;

void 
files_set_executable(char *execname)
{
  executable_name = strdup(basename(execname));
  if (executable_name[0] != '/') {
    char path[PATH_MAX_LEN];
    // check return code; use pathname in fnbounds_static epoch finalize
    realpath(executable_name, path);
    executable_pathname = strdup(path);
  }
  else {
    executable_pathname = executable_name;
  }
}


//***************************************************************
// private operations
//***************************************************************


static long
os_hostid()
{
  static long hostid = NO_HOST_ID;

  if (hostid == NO_HOST_ID) hostid = gethostid();

  return hostid;
}


static unsigned int
os_pid()
{
  static unsigned int pid = NO_PID;

  if (pid == NO_PID) pid = getpid();

  return pid;
}

static char *
os_realpath(const char *inpath, char *outpath)
{
   return realpath(inpath, outpath);
}


static char *
files_name(char *filename, unsigned int mpi, const char *suffix)
{
  thread_data_t *td = csprof_get_thread_data();

  sprintf(filename, "%s/%s-%06u-%03lu-%lx-%u.%s", 
          output_directory, executable_name, mpi, 
          td->state->pstate.thrid,
          os_hostid(), os_pid(), suffix); 

  return filename;
}
