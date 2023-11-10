#ifndef blame_shift_opencl_h
#define blame_shift_opencl_h

//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../../lib/prof-lean/hpcrun-opencl.h"      // cl_event, cl_command_queue



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_queue_prologue
(
 cl_command_queue queue
);


void
opencl_queue_epilogue
(
 cl_command_queue queue
);


void
opencl_kernel_prologue
(
 cl_event event,
 uint32_t kernel_module_id
);


void
opencl_kernel_epilogue
(
 cl_event event
);


void
opencl_sync_prologue
(
 cl_command_queue queue
);


void
opencl_sync_epilogue
(
 cl_command_queue queue,
 uint16_t num_sync_events
);

#endif  //blame_shift_opencl_h
