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
convert_kernel_launch
(
  gpu_activity_t *ga,
  opencl_object_t *cb_data,
  cl_event event
)
{
  memset(&ga->details.kernel, 0, sizeof(gpu_kernel_t));
  getTimingInfoFromClEvent(&ga->details.interval, event);

  ga->kind     = cb_data->kind;
  ga->cct_node = cb_data->details.cct_node;

  ga->details.kernel.correlation_id = cb_data->details.ker_cb.correlation_id;
  ga->details.kernel.submit_time    = cb_data->details.submit_time;

}


static void
convert_memcpy
(
  gpu_activity_t *ga,
  opencl_object_t *cb_data,
  cl_event event
)
{
  memset(&ga->details.memcpy, 0, sizeof(gpu_memcpy_t));
  getTimingInfoFromClEvent(&ga->details.interval, event);

  ga->kind     = cb_data->kind;
  ga->cct_node = cb_data->details.cct_node;

  ga->details.memcpy.correlation_id  = cb_data->details.mem_cb.correlation_id;
  ga->details.memcpy.submit_time     = cb_data->details.submit_time;
  ga->details.memcpy.bytes           = cb_data->details.mem_cb.size;
  ga->details.memcpy.copyKind        = cb_data->details.mem_cb.type;
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_activity_translate
(
  gpu_activity_t *ga,
  cl_event event,
  opencl_object_t *cb_data
)
{
  switch (cb_data->kind) {
    case GPU_ACTIVITY_MEMCPY:
      convert_memcpy(ga, cb_data, event);
      break;

    case GPU_ACTIVITY_KERNEL:
      convert_kernel_launch(ga, cb_data, event);
      break;

    default:
      assert(0);
  }
  cstack_ptr_set(&(ga->next), 0);
}
