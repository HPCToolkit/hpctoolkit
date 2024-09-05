// SPDX-FileCopyrightText: 2022-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _OPTIMIZATION_CHECK_H_
#define _OPTIMIZATION_CHECK_H_

//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <CL/cl.h>



//******************************************************************************
// interface functions
//******************************************************************************

void
isQueueInInOrderExecutionMode
(
        const cl_command_queue_properties *properties
);


void
recordQueueContext
(
 cl_command_queue queue,
 cl_context context
);


void
clearQueueContext
(
 cl_command_queue queue
);


void
isKernelSubmittedToMultipleQueues
(
 cl_kernel kernel,
 cl_command_queue queue
);


void
clearKernelQueues
(
 cl_kernel kernel
);


void
recordKernelParams
(
 cl_kernel kernel,
 const void *mem,
 size_t size
);


void
areKernelParamsAliased
(
 cl_kernel kernel,
 uint32_t kernel_module_id
);


void
clearKernelParams
(
 cl_kernel kernel
);


void
recordDeviceCount
(
 unsigned int num_devices,
 const cl_device_id *devices
);


void
isSingleDeviceUsed
(
 void
);


void
recordH2DCall
(
 cl_mem buffer
);


void
recordD2HCall
(
 cl_mem buffer
);


void
clearBufferEntry
(
 cl_mem buffer
);


void
areAllDevicesUsed
(
  unsigned int totalDeviceCount
);

#endif  // _OPTIMIZATION_CHECK_H_
