//*****************************************************************************
// File: messages-sync.c 
//
// Description:
//   This half of the messaging system. It contains parts that need not be
//   async-signal safe. 
//
// History:
//   19 July 2009 
//     created by partitioning pmsg.c into pmsg-sync.c and pmsg-async.c
//
//*****************************************************************************


//*****************************************************************************
// global includes 
//*****************************************************************************

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>



//*****************************************************************************
// local includes 
//*****************************************************************************

#include "disabled.h"
#include "fname_max.h"
#include "files.h"
#include "name.h"
#include "thread_data.h"
#include "thread_use.h"
#include "monitor.h"

#include "messages/debug-flag.h"
#include "messages/messages.h"
#include "messages/messages.i"


//*****************************************************************************
// global variables 
//*****************************************************************************

FILE *log_file;


//-------------------------------------
// Log output may be throttled by using 
// the message limiting mechanism
//-------------------------------------
static int global_msg_count = 0;

//-------------------------------------
// how many unwind msg blocks to permit 
// (500 is reasonable choice)
// FIXME: make this an option
//-------------------------------------
static int const threshold = 500;



//*****************************************************************************
// forward declarations 
//*****************************************************************************




//*****************************************************************************
// interface operations 
//*****************************************************************************

void
messages_init()
{
  debug_flag_init();

  spinlock_unlock(&pmsg_lock); // initialize lock for async operations

  log_file = stderr;
}


void
messages_logfile_create()
{
  if (hpcrun_get_disabled()) return;

  // get name for log file 
  char log_name[PATH_MAX];
  files_log_name(log_name, 0, PATH_MAX);

  // open log file
  log_file = fopen(log_name,"w");
  if (!log_file) log_file = stderr; // reset to stderr
}


void
messages_fini(void)
{
  if (hpcrun_get_disabled()) return;

  if (log_file != stderr){
    fclose(log_file);

    //----------------------------------------------------------------------
    // if this is an execution of an MPI program, we opened the log file 
    // before the MPI rank was known. thus, the name of the log file is 
    // missing the MPI rank. fix that now by renaming the log file to what 
    // it should be.
    //----------------------------------------------------------------------
    int rank = monitor_mpi_comm_rank();
    if (rank >= 0) {
      char old[PATH_MAX];
      char new[PATH_MAX];
      files_log_name(old, 0, PATH_MAX);
      files_log_name(new, rank, PATH_MAX);
      rename(old, new);
    }
  }
}


void
csprof_exit_on_error(int ret, int ret_expected, const char *fmt, ...)
{
  if (ret == ret_expected) {
    return;
  }
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
  abort();
}


void
csprof_abort_w_info(void (*info)(void), const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - strlen(fstr) - 5);
  strcat(fstr,"\n");

  va_list args;

  if (log_file != stderr) {
    va_start(args, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
  }

  va_start(args,fmt);
  vfprintf(stderr, fstr, args);
  va_end(args);
  info();
  monitor_real_exit(-1);
}


// message to log file, also echo on stderr
void
csprof_stderr_log_msg(bool copy_to_log, const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];
  va_list args;

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - 5);
  strcat(fstr,"\n");

  va_start(args, fmt);
  vfprintf(stderr, fstr, args);
  va_end(args);

  if (copy_to_log && log_file != stderr){
    va_list args;
    va_start(args, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
  }
}


void
messages_donothing(void)
{
}


int
messages_logfile_fd(void)
{
  return fileno(log_file);
}


int
csprof_below_pmsg_threshold(void)
{
  return (global_msg_count < threshold);
}


void
csprof_up_pmsg_count(void)
{
  global_msg_count++;
}

