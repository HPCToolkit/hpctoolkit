// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
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

//***************************************************************************
//
// File:
//   gpu-flush-alarm.c
//
// Purpose:
//   manage an alarm that goes off in if gpu operation finalization doesn't
//   properly finish. this allows measurement data already received to be
//   recorded and the application to finish instead of hanging forever.
//
//***************************************************************************


//******************************************************************************
// system includes
//******************************************************************************

#include <pthread.h>
#include <unistd.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-flush-alarm.h"
#include <monitor.h>
#include <messages/messages.h>
#include <utilities/linuxtimer.h>



//******************************************************************************
// macros
//******************************************************************************

#define GPU_FLUSH_ALARM_TIMEOUT_SECONDS 4


//******************************************************************************
// global variables
//******************************************************************************

__thread jmp_buf gpu_flush_alarm_jump_buf;



//******************************************************************************
// local variables
//******************************************************************************

// declaring these variables thread local will mean that multiple
// flush alarms can be active at the same time.

static __thread const char *gpu_flush_alarm_msg;
static __thread linuxtimer_t gpu_flush_alarm_timer;

static pthread_once_t gpu_flush_alarm_initialized = PTHREAD_ONCE_INIT;
static int gpu_flush_alarm_signal;


//******************************************************************************
// private operations
//******************************************************************************

static int
gpu_flush_alarm_handler
(
 int sig,
 siginfo_t* siginfo,
 void* context
)
{
  STDERR_MSG(gpu_flush_alarm_msg);
  longjmp(gpu_flush_alarm_jump_buf, 1);
  return 0; /* keep compiler happy, but can't get here */
}



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_flush_alarm_init
(
)
{
  gpu_flush_alarm_signal = linuxtimer_newsignal();
}


void
gpu_flush_alarm_set
(
 const char *client_msg
)
{
  pthread_once(&gpu_flush_alarm_initialized, gpu_flush_alarm_init);

  gpu_flush_alarm_msg = client_msg;

  linuxtimer_create(&gpu_flush_alarm_timer, CLOCK_REALTIME,
		    gpu_flush_alarm_signal);

  monitor_sigaction(linuxtimer_getsignal(&gpu_flush_alarm_timer),
		    &gpu_flush_alarm_handler, 0, NULL);

  linuxtimer_set(&gpu_flush_alarm_timer, GPU_FLUSH_ALARM_TIMEOUT_SECONDS, 0, 0);
}


void
gpu_flush_alarm_clear
(
 void
)
{
  linuxtimer_set(&gpu_flush_alarm_timer, 0, 0, 0);
  linuxtimer_delete(&gpu_flush_alarm_timer);
}

void
gpu_flush_alarm_test
(
 void
)
{
  sleep(20);
}
