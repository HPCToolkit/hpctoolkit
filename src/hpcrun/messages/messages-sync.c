// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../disabled.h"
#include "../fname_max.h"
#include "../files.h"
#include "../name.h"
#include "../rank.h"
#include "../thread_data.h"
#include "../thread_use.h"
#include "../audit/audit-api.h"

#include "debug-flag.h"
#include "messages.h"
#include "messages-internal.h"
#include "fmt.h"


//*****************************************************************************
// macros
//*****************************************************************************

#define STDERR_FD 2


//*****************************************************************************
// file local (static) variables
//*****************************************************************************

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

static int log_file_fd = STDERR_FD;

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
  global_msg_count = 0;

  spinlock_init(&pmsg_lock); // initialize lock for async operations

  log_file_fd = STDERR_FD;      // std unix stderr
}


void
messages_logfile_create()
{
  if (hpcrun_get_disabled()) return;

  // open log file
  if (getenv("HPCRUN_LOG_STDERR") != NULL) {
    // HPCRUN_LOG_STDERR variable set ==> log goes to stderr
    log_file_fd = STDERR_FD;
  }
  else {
    // Normal case of opening .log file.
    log_file_fd = hpcrun_open_log_file();
  }
  if (log_file_fd == -1) {
    log_file_fd = STDERR_FD; // cannot open log_file ==> revert to stderr
  }
}


void
messages_fini(void)
{
  if (hpcrun_get_disabled()) return;

  if (log_file_fd != STDERR_FD) {
    int rv = close(log_file_fd);
    if (rv) {
      char executable[PATH_MAX];
      char *exec = realpath("/proc/self/exe", executable);
      unsigned long pid = (unsigned long) getpid();

      STDERR_MSG("hpctoolkit warning: executable '%s' (pid=%ld) "
              "prematurely closed hpctoolkit's log file", exec, pid);
    }
    //----------------------------------------------------------------------
    // if this is an execution of an MPI program, we opened the log file
    // before the MPI rank was known. thus, the name of the log file is
    // missing the MPI rank. fix that now by renaming the log file to what
    // it should be.
    //----------------------------------------------------------------------
    int rank = hpcrun_get_rank();
    if (rank >= 0) {
      hpcrun_rename_log_file(rank);
    }
  }
}


void
hpcrun_exit_on_error(int ret, int ret_expected, const char *fmt, ...)
{
  if (ret == ret_expected) {
    return;
  }
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, &box);
  hpcrun_terminate();
}


void
hpcrun_abort_w_info(void (*info)(void), const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - strlen(fstr) - 5);
  strcat(fstr,"\n");

  va_list_box box;

  if (log_file_fd != STDERR_FD) {
    va_list_box_start(box, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, &box);
  }

  char buf[1024] = "";
  va_list_box_start(box, fmt);
  hpcrun_msg_vns(buf, sizeof(buf), fstr, &box);
  write(STDERR_FD, buf, strlen(buf));
  va_list_box_end(box);
  info();
  auditor_exports->exit(-1);
}

// message to log file, also echo on stderr
void
hpcrun_stderr_log_msg(bool copy_to_log, const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];
  va_list_box box;

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - 5);
  strcat(fstr,"\n");

  char buf[2048] = "";
  va_list_box_start(box, fmt);
  hpcrun_msg_vns(buf, sizeof(buf), fstr, &box);
  write(STDERR_FD, buf, strlen(buf));
  va_list_box_end(box);

  if (copy_to_log && log_file_fd != STDERR_FD){
    va_list_box_start(box, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, &box);
  }
}


void
messages_donothing(void)
{
}


int
messages_logfile_fd(void)
{
  return log_file_fd;
}


int
hpcrun_below_pmsg_threshold(void)
{
  return (global_msg_count < threshold);
}


void
hpcrun_up_pmsg_count(void)
{
  global_msg_count++;
}
