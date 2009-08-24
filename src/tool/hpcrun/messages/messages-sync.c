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
// Copyright ((c)) 2002-2009, Rice University 
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

#include "disabled.h"
#include "fname_max.h"
#include "files.h"
#include "name.h"
#include "thread_data.h"
#include "thread_use.h"
#include "monitor.h"

#include <messages/debug-flag.h>
#include <messages/messages.h>
#include <messages/messages.i>
#include <messages/fmt.h>


//*****************************************************************************
// global variables 
//*****************************************************************************

FILE *log_file;


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

static int log_file_fd = 2;   // initially log_file is stderr

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

  log_file    = stderr;
  log_file_fd = 2;      // std unix stderr
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
  if (!log_file) {
    log_file = stderr; // reset to stderr
    log_file_fd = 2;
  }
  else {
    log_file_fd = fileno(log_file);
  }
#if 0
  log_file_fd = open(log_name, O_RDWR | O_CREAT);
  if (log_file_fd == -1) {
    log_file_fd = 2; // cannot open log_file ==> revert to stderr
  }
#endif
}


void
messages_fini(void)
{
  if (hpcrun_get_disabled()) return;

#if 0
  if (log_file_fd != 2) {
    int rv = close(log_file_fd);
    if (rv) {
      static char close_err[] = "An error occurred during the close of the log file! Be warned!\n";
      write(2, close_err, strlen(close_err));
    }
#else
  if (log_file != stderr){
    fclose(log_file);
#endif

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
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, &box);
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

  va_list_box box;

  if (log_file != stderr) {
    va_list_box_start(box, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, &box);
  }

  char buf[1024] = "";
  va_list_box_start(box, fmt);
  hpcrun_msg_vns(buf, sizeof(buf), fstr, &box);
  write(2, buf, strlen(buf));
  va_list_box_end(box);
  info();
  monitor_real_exit(-1);
}


// message to log file, also echo on stderr
void
csprof_stderr_log_msg(bool copy_to_log, const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];
  va_list_box box;

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - 5);
  strcat(fstr,"\n");

  char buf[1024] = "";
  va_list_box_start(box, fmt);
  hpcrun_msg_vns(buf, sizeof(buf), fstr, &box);
  write(2, buf, strlen(buf));
  va_list_box_end(box);

  if (copy_to_log && log_file != stderr){
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
csprof_below_pmsg_threshold(void)
{
  return (global_msg_count < threshold);
}


void
csprof_up_pmsg_count(void)
{
  global_msg_count++;
}

