// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
