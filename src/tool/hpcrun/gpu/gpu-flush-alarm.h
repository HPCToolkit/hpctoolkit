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
// Copyright ((c)) 2002-2023, Rice University
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
//   gpu-flush-alarm.h
//
// Purpose:
//   manage an alarm that goes off in if gpu operation finalization doesn't
//   properly finish. this allows measurement data already received to be
//   recorded and the application to finish instead of hanging forever.
//
//***************************************************************************

#ifndef __gpu_flush_alarm__
#define __gpu_flush_alarm__

//******************************************************************************
// system includes
//******************************************************************************

#include <setjmp.h>



//******************************************************************************
// macros
//******************************************************************************

#if GPU_FLUSH_ALARM_ENABLED

//----------------------------------------------
// flush alarm enabled
//----------------------------------------------
#define GPU_FLUSH_ALARM_SET(msg)                \
  gpu_flush_alarm_set(msg)
#define GPU_FLUSH_ALARM_CLEAR()                 \
  gpu_flush_alarm_clear()
#define GPU_FLUSH_ALARM_FIRED()                 \
  setjmp(gpu_flush_alarm_jump_buf)

#else

//----------------------------------------------
// flush alarm disabled
//----------------------------------------------
#define GPU_FLUSH_ALARM_INIT()
#define GPU_FLUSH_ALARM_SET()
#define GPU_FLUSH_ALARM_CLEAR()
#define GPU_FLUSH_ALARM_FIRED() 0
#define GPU_FLUSH_ALARM_FINI()

#endif

#if GPU_FLUSH_ALARM_TEST_ENABLED
#define GPU_FLUSH_ALARM_TEST()                  \
  gpu_flush_alarm_test()
#else
#define GPU_FLUSH_ALARM_TEST()
#endif



//******************************************************************************
// global variable external declarations
//******************************************************************************

extern __thread jmp_buf gpu_flush_alarm_jump_buf;



void
gpu_flush_alarm_init
(
 void
);


void
gpu_flush_alarm_set
(
 const char *client_msg
);


void
gpu_flush_alarm_clear
(
 void
);


void
gpu_flush_alarm_test
(
 void
);



#endif
