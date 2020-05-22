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

#include <hpcrun/messages/messages.h>

#include "opencl-activity-translate.h"
#include "opencl-api.h"
#include "opencl-memory-manager.h"



//******************************************************************************
// private operations
//******************************************************************************

static void
getTimingInfoFromClEvent
(
  profilingData_t* pd,
  cl_event event
)
{
  cl_ulong commandStart = 0;
  cl_ulong commandEnd = 0;
  cl_int errorCode = CL_SUCCESS;

  errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(commandStart), &commandStart, NULL);
  errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(commandEnd), &commandEnd, NULL);
  if (errorCode != CL_SUCCESS) {
	ETMSG(CL, "error in collecting profiling data");
  }
  pd->startTime = commandStart;
  pd->endTime = commandEnd;
}


static void
getMemoryProfileInfo
(
  profilingData_t* pd,
  cl_memory_callback_t* cb_data
)
{
  pd->size = cb_data->size;
  pd->fromHostToDevice = cb_data->fromHostToDevice;
  pd->fromDeviceToHost = cb_data->fromDeviceToHost;
}


static void
openclKernelDataToGenericKernelData
(
  gpu_kernel_t *generic_data,
  profilingData_t *pd
)
{
  generic_data->start = (uint64_t)pd->startTime;
  generic_data->end = (uint64_t)pd->endTime;
}


static void
openclMemDataToGenericMemData
(
  gpu_memcpy_t *generic_data,
  profilingData_t *pd
)
{
  generic_data->start = (uint64_t)pd->startTime;
  generic_data->end = (uint64_t)pd->endTime;
  generic_data->bytes = (uint64_t)pd->size;
  generic_data->copyKind = (gpu_memcpy_type_t) (pd->fromHostToDevice)? GPU_MEMCPY_H2D: pd->fromDeviceToHost? GPU_MEMCPY_D2H:
	GPU_MEMCPY_UNK;
}


static void
convert_kernel_launch
(
  gpu_activity_t * ga,
  void * user_data,
  cl_event event
)
{
  cl_kernel_callback_t* kernel_cb_data = (cl_kernel_callback_t*)user_data;
  ETMSG(CL, "Saving kernel data to gpu_activity_t");
  ga->kind = GPU_ACTIVITY_KERNEL;
  opencl_object_t *p = opencl_malloc();
  profilingData_t* pd = &(p->details.pd);
  getTimingInfoFromClEvent(pd, event);
  gpu_kernel_t kernel_data;
  memset(&kernel_data, 0, sizeof(gpu_kernel_t));
  openclKernelDataToGenericKernelData(&kernel_data, pd);
  ga->details.kernel = kernel_data;
  set_gpu_interval(&ga->details.interval, pd->startTime, pd->endTime);
  ga->details.kernel.correlation_id = kernel_cb_data->correlation_id;
  opencl_free(p);
}


static void
convert_memcpy
(
  gpu_activity_t * ga,
  void * user_data,
  cl_event event,
  gpu_memcpy_type_t kind
)
{
  cl_memory_callback_t* memory_cb_data = (cl_memory_callback_t*)user_data;
  opencl_object_t *p = opencl_malloc();
  profilingData_t* pd = &(p->details.pd);
  getTimingInfoFromClEvent(pd, event);
  getMemoryProfileInfo(pd, memory_cb_data);
  ga->kind = GPU_ACTIVITY_MEMCPY;
  gpu_memcpy_t memcpy_data;
  memset(&memcpy_data, 0, sizeof(gpu_memcpy_t));
  openclMemDataToGenericMemData(&memcpy_data, pd);
  ga->details.memcpy = memcpy_data;
  ga->details.memcpy.correlation_id = memory_cb_data->correlation_id;
  set_gpu_interval(&ga->details.interval, pd->startTime, pd->endTime);
  opencl_free(p);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_activity_translate
(
  gpu_activity_t * ga,
  cl_event event,
  void * user_data
)
{
  cl_generic_callback_t* cb_data = (cl_generic_callback_t*)user_data;
  opencl_call type = cb_data->type;
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
