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
// Copyright ((c)) 2002-2020, Rice University
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
//   libomptarget-api.c
//
// Purpose:
//   wrapper around LLVM libomptarget layer
//
//***************************************************************************


//*****************************************************************************
// system include files
//*****************************************************************************

#include <pthread.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include <hpcrun/main.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/sample-sources/libdl.h>

#include <monitor.h>



//******************************************************************************
// static data
//******************************************************************************

static void (*__tgt_register_lib_fn)(void *);



//******************************************************************************
// private operations
//******************************************************************************


static int
bind
(
  void
)
{
  CHK_DLOPEN(libomptarget, "libomptarget.so", RTLD_NOW | RTLD_GLOBAL);
  CHK_DLSYM(libomptarget, __tgt_register_lib);
  return 0;
}


static void
init
(
  void
)
{
  if (bind()) {
    EEMSG("hpcrun: unable to bind to libomptarget library %s\n", dlerror());
    monitor_real_exit(-1);
  }
  monitor_initialize();
  hpcrun_prepare_measurement_subsystem(false);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
__tgt_register_lib
(
  void *arg
)
{
  // libomptarget - a device-independent library for
  // OpenMP offloading uses this function to register GPU
  // code. When AMD's OpenMP GPU runtime initializes itself,
  // it creates threads while holding locks, which makes it
  // awkward to initialize HPCToolkit's support for monitoring
  // OpenMP offload on AMD GPUs. HPCToolkit invokes some AMD
  // runtime functions that try to acquire the same locks
  // already held, which causes deadlock. Avoid this problem
  // by forcing initialization of HPCToolkit's measurement
  // subsystem before offloading GPU code.

  static pthread_once_t once_control = PTHREAD_ONCE_INIT;
  pthread_once(&once_control, init);

  __tgt_register_lib_fn(arg);
}
