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

#include <assert.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-activity.h>

#include "opencl-activity-translate.h"
#include "opencl-api.h"
#include "opencl-intercept.h"



//******************************************************************************
// private operations
//******************************************************************************

static void
getMemoryProfileInfo
(
  gpu_memcpy_t *memcpy,
  cl_memory_callback_t *cb_data
)
{
  memcpy->correlation_id = cb_data->correlation_id;
  memcpy->bytes = cb_data->size;
  memcpy->copyKind = (gpu_memcpy_type_t) 
    (cb_data->fromHostToDevice)? GPU_MEMCPY_H2D: 
    (cb_data->fromDeviceToHost? GPU_MEMCPY_D2H:	GPU_MEMCPY_UNK);
}


static void
convert_kernel_launch
(
  gpu_activity_t *ga,
  void *user_data,
  cl_event event
)
{
  cl_kernel_callback_t *kernel_cb_data = (cl_kernel_callback_t*)user_data;
  memset(&ga->details.kernel, 0, sizeof(gpu_kernel_t));
  getTimingInfoFromClEvent(&ga->details.interval, event);
  ga->kind = GPU_ACTIVITY_KERNEL;
  ga->details.kernel.correlation_id = kernel_cb_data->correlation_id;
}


static void
convert_memcpy
(
  gpu_activity_t *ga,
  void *user_data,
  cl_event event,
  gpu_memcpy_type_t kind
)
{
  cl_memory_callback_t *memory_cb_data = (cl_memory_callback_t*)user_data;
  memset(&ga->details.memcpy, 0, sizeof(gpu_memcpy_t));
  getTimingInfoFromClEvent(&ga->details.interval, event);
  getMemoryProfileInfo(&ga->details.memcpy, memory_cb_data);
  ga->kind = GPU_ACTIVITY_MEMCPY;
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_activity_translate
(
  gpu_activity_t *ga,
  cl_event event,
  void *user_data
)
{
  cl_generic_callback_t *cb_data = (cl_generic_callback_t*)user_data;
  opencl_call_t type = cb_data->type;
  switch (type) {
    case kernel:
      convert_kernel_launch(ga, user_data, event);
      break;
    case memcpy_H2D:
      convert_memcpy(ga, user_data, event, GPU_MEMCPY_H2D);
      break;
    case memcpy_D2H:
      convert_memcpy(ga, user_data, event, GPU_MEMCPY_D2H);
      break;
    default:
      assert(0);
  }
  cstack_ptr_set(&(ga->next), 0);
}
