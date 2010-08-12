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

//*****************************************************************************
// File: messages-async.c 
//
// Description:
//   This half of the messaging system must be async-signal safe.
//   Namely, it must be safe to call these operations from within signal 
//   handlers. Operations in this file may not directly or indirectly 
//   allocate memory using malloc. They may not call functions that
//   acquire locks that may be already held by the application code or 
//   functions that may be called during synchronous profiler operations
//   that may be interrupted by asynchronous profiler operations.
//
// History:
//   19 July 2009 
//     created by partitioning pmsg.c into messages-sync.c and 
//     messages-async.c
//
//*****************************************************************************



//*****************************************************************************
// global includes 
//*****************************************************************************

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>



//*****************************************************************************
// local includes 
//*****************************************************************************

#include "disabled.h"
#include "messages.h"
#include "messages.i"
#include "fmt.h"
#include "sample_event.h"
#include "thread_data.h"
#include "thread_use.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG_PMSG_ASYNC 0



//*****************************************************************************
// global variables
//*****************************************************************************

spinlock_t pmsg_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// forward declarations
//*****************************************************************************

static void create_msg(char *buf, size_t buflen, bool add_thread_id, 
		       const char *tag, const char *fmt, va_list_box* box);


//*****************************************************************************
// interface operations
//*****************************************************************************

void
hpcrun_emsg_valist(const char *fmt, va_list_box* box)
{
  hpcrun_write_msg_to_log(false, false, NULL, fmt, box);
}


void
hpcrun_emsg(const char *fmt,...)
{
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(false, true, NULL, fmt, &box);
}


void
hpcrun_pmsg(pmsg_category flag, const char *tag, const char *fmt, ...)
{
  if (debug_flag_get(flag) == 0){
#if DEBUG_PMSG_ASYNC
    hpcrun_emsg("PMSG flag in = %d (%s), flag ctl = %d --> NOPRINT",
		flag, tbl[flag], debug_flag_get(flag));
#endif
    return;
  }
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(false, true, tag, fmt, &box);
}


// like pmsg, except echo message to stderr when flag is set
void
hpcrun_pmsg_stderr(bool echo_stderr, pmsg_category flag, const char *tag, 
		   const char *fmt ,...)
{
  if (debug_flag_get(flag) == 0){
    return;
  }
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(echo_stderr, true, tag, fmt, &box);
}


// like nmsg, except echo message to stderr when flag is set
void
hpcrun_nmsg_stderr(bool echo_stderr, pmsg_category flag, const char *tag, 
		   const char *fmt ,...)
{
  if (debug_flag_get(flag) == 0){
    return;
  }
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(echo_stderr, false, tag, fmt, &box);
}


void
hpcrun_nmsg(pmsg_category flag, const char* tag, const char *fmt, ...)
{
  if (debug_flag_get(flag) == 0){
    return;
  }
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(false, false, tag, fmt, &box);
}


void
hpcrun_amsg(const char *fmt,...)
{
  va_list_box box;
  va_list_box_start(box, fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, &box);
}



//*****************************************************************************
// interface operations (within message subsystem) 
//*****************************************************************************

void
hpcrun_write_msg_to_log(bool echo_stderr, bool add_thread_id, const char *tag,
			const char *fmt, va_list_box* box)
{
  char buf[MSG_BUF_SIZE];

  if (hpcrun_get_disabled() && (! echo_stderr)){
    return;
  }

  create_msg(&buf[0], sizeof(buf), add_thread_id, tag, fmt, box);
  va_list_boxp_end(box);

  if (echo_stderr){
    write(2, buf, strlen(buf));
  }

  if (hpcrun_get_disabled()) return;

  spinlock_lock(&pmsg_lock);

  // use write to logfile file descriptor, instead of fprintf stuff
  //
  write(messages_logfile_fd(), buf, strlen(buf));

  spinlock_unlock(&pmsg_lock);
}


//*****************************************************************************
// private operations
//*****************************************************************************

static void
create_msg(char *buf, size_t buflen, bool add_thread_id, const char *tag,
	   const char *fmt, va_list_box* box)
{
  char fstr[MSG_BUF_SIZE];
  int  tmp_id = -1;

  fstr[0] = '\0';

  if (add_thread_id) {
    if (hpcrun_using_threads_p()) {
      tmp_id = TD_GET(id);
      hpcrun_msg_ns(fstr, sizeof(fstr), "[%d]: ", tmp_id);
    }
  }
  if (ENABLED(PID)) {
    hpcrun_msg_ns(fstr, sizeof(fstr), "[%d]: ", getpid());
  }

  if (tag) {
    char* fstr_end = fstr + strlen(fstr);
    hpcrun_msg_ns(fstr_end, sizeof(fstr) - strlen(fstr), "%-5s: ", tag);
  }

  strncat(fstr, fmt, MSG_BUF_SIZE - strlen(fstr) - 5);
  strcat(fstr,"\n");

  hpcrun_msg_vns(buf, buflen - 2, fstr, box);
}

