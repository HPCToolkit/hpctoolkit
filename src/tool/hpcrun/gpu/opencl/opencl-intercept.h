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

#ifndef _OPENCL_INTERCEPT_H_
#define _OPENCL_INTERCEPT_H_



//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-opencl.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef cl_command_queue (*clqueue_t)(
  cl_context,
  cl_device_id,
  cl_command_queue_properties,
  cl_int *
);


typedef cl_int (*clkernel_t)(
  cl_command_queue,
  cl_kernel,
  cl_uint,
  const size_t *,
  const size_t *,
  const size_t *,
  cl_uint,
  const cl_event *,
  cl_event *
);


typedef cl_int (*clreadbuffer_t)(
  cl_command_queue,
  cl_mem,
  cl_bool,
  size_t,
  size_t,
  void *,
  cl_uint,
  const cl_event *,
  cl_event *
);


typedef cl_int (*clwritebuffer_t)(
  cl_command_queue,
  cl_mem,
  cl_bool,
  size_t,
  size_t,
  const void *,
  cl_uint,
  const cl_event *,
  cl_event *
);


typedef enum {
  memcpy_H2D                      = 0,
  memcpy_D2H                      = 1,
  kernel                          = 2
} opencl_call_t;


typedef struct cl_generic_callback_t {
  uint64_t correlation_id;
  opencl_call_t type;
} cl_generic_callback_t;


typedef struct cl_kernel_callback_t {
  uint64_t correlation_id;
  opencl_call_t type;
} cl_kernel_callback_t;


typedef struct cl_memory_callback_t {
  uint64_t correlation_id;
  opencl_call_t type;
  bool fromHostToDevice;
  bool fromDeviceToHost;
  size_t size;
} cl_memory_callback_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_intercept_setup
(
  void
);


void
opencl_intercept_teardown
(
  void
);



#endif  //_OPENCL_INTERCEPT_H_
