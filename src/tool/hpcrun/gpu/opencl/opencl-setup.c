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

//******************************************************************************
// system includes
//******************************************************************************

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/main.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/sample-sources/libdl.h>

#include "opencl-api.h"
#include "opencl-intercept.h"
#include "opencl-setup.h"



//******************************************************************************
// macros
//******************************************************************************

#define FORALL_OPENCL_ROUTINES(macro)	\
  macro(clCreateCommandQueue)   		\
  macro(clEnqueueNDRangeKernel)			\
  macro(clEnqueueWriteBuffer)  			\
  macro(clEnqueueReadBuffer)

#define OPENCL_FN_NAME(f) DYN_FN_NAME(f)

#define OPENCL_FN(fn, args) \
  static cl_int (*OPENCL_FN_NAME(fn)) args

OPENCL_FN
(
  clCreateCommandQueue, (cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret)
);

OPENCL_FN
(
  clEnqueueNDRangeKernel, (cl_command_queue command_queue, cl_kernel ocl_kernel, cl_uint work_dim, const size_t *global_work_offset, 													   const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, 															 const cl_event *event_wait_list, cl_event *event)
);

OPENCL_FN
(
  clEnqueueReadBuffer, (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb,															 void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
);

OPENCL_FN
(
  clEnqueueWriteBuffer, (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb,														  const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
);



//******************************************************************************
// private operations
//******************************************************************************

static const char*
opencl_path
(
  void
)
{
  const char *path = "libOpenCL.so";
  return path;
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_initialize
(
  void
)
{
  ETMSG(CL, "We are setting up opencl intercepts");
  setup_opencl_intercept();
  initialize_opencl_operation_count();
  initialize_opencl_correlation_id();
}


int
opencl_bind
(
  void
)
{
  // This is a workaround so that we do not hang when taking timer interrupts
  setenv("HSA_ENABLE_INTERRUPT", "0", 1);

  #ifndef HPCRUN_STATIC_LINK  // dynamic libraries only availabile in non-static case
    hpcrun_force_dlopen(true);
    CHK_DLOPEN(opencl, opencl_path(), RTLD_NOW | RTLD_GLOBAL);
    hpcrun_force_dlopen(false);

    #define OPENCL_BIND(fn) \
      CHK_DLSYM(opencl, fn);

    FORALL_OPENCL_ROUTINES(OPENCL_BIND)

    #undef OPENCL_BIND

	return 0;
  #else
    return -1;
  #endif // ! HPCRUN_STATIC_LINK  
}


/*
void opencl_finalize()
{
  teardown_opencl_intercept();
}*/
