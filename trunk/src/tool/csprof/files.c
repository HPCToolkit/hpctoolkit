//***************************************************************
// global includes 
//***************************************************************

#include <errno.h>   // errno
#include <limits.h>  // PATH_MAX
#include <stdio.h>   // sprintf
#include <stdlib.h>  // realpath
#include <string.h>  // strerror
#include <unistd.h>  // gethostid



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

static char *files_name(char *filename, const char *suffix);

static unsigned int os_pid();
static long os_hostid();
static char *os_realpath(const char *inpath, char *outpath);



//***************************************************************
// local data 
//***************************************************************

char output_directory[PATH_MAX];
char *executable_name = 0;



//***************************************************************
// interface operations
//***************************************************************

void 
files_trace_name(char *filename, int len)
{
  files_name(filename, CSPROF_TRACE_FNM_SFX);
}


void
files_profile_name(char *filename, int len)
{
  files_name(filename, CSPROF_PROFILE_FNM_SFX);
}


void
files_log_name(char *filename, int len)
{
  sprintf(filename, "%s/%s-%lx-%u.%s", 
	  output_directory, executable_name, os_hostid(), 
	  os_pid(), CSPROF_LOG_FNM_SFX);
}


void 
files_set_directory()
{  
  char *path = getenv(CSPROF_OPT_OUT_PATH);

  if (path == NULL || strlen(path) == 0) path = ".";

  if (os_realpath(path, output_directory) == NULL) {
    EMSG("could not access path `%s': %s", path, strerror(errno));
    abort();
  }
}


void 
files_set_executable(char *execname)
{
  executable_name = strdup(basename(execname));
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
files_name(char *filename, const char *suffix)
{
  thread_data_t *td = csprof_get_thread_data();

  sprintf(filename, "%s/%s-%lx-%u-%lu.%s", 
	  output_directory, executable_name, os_hostid(), 
	  os_pid(), td->state->pstate.thrid,
	  suffix); 

  return filename;
}
