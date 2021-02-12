#ifndef _OPTIMIZATION_CHECK_H_
#define _OPTIMIZATION_CHECK_H_

//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-opencl.h>



//******************************************************************************
// interface functions
//******************************************************************************

void
optimization_metrics_enable
(
 void
);


void
isQueueInInOrderExecutionMode
(
	cl_command_queue_properties properties
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
 void *mem,
 size_t size
);


void
clearKernelParams
(
 cl_kernel kernel
);


void
recordDeviceCount
(
  uint num_devices
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

#endif  // _OPTIMIZATION_CHECK_H_
