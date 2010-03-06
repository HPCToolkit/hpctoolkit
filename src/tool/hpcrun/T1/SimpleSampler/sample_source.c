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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */

#include "monitor.h"
#include "sample_source.h"
#include "sample_handler.h"

static int
itimer_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  process_sample(context);
  start_sample_source();
}

static struct itimerval itimer;

static int seconds = 0;
static int microseconds = 5000;

void
init_sample_source(void)
{
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  itimer.it_interval.tv_sec = seconds;
  itimer.it_interval.tv_usec = microseconds;

  monitor_sigaction(SIGPROF, &itimer_signal_handler, 0, NULL);
}

void
start_sample_source(void)
{
  int rv = setitimer(ITIMER_PROF, &itimer, NULL);
}

void
stop_sample_source(void)
{
  struct itimerval itimer;

  timerclear(&itimer.it_value);
  timerclear(&itimer.it_interval);
  int rc = setitimer(ITIMER_PROF, &itimer, NULL);
}

void
shutdown_sample_source(void)
{
  ;
}
