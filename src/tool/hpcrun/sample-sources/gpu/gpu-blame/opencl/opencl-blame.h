#ifndef blame_shift_opencl_h
#define blame_shift_opencl_h

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>										// cct_node_t
#include <lib/prof-lean/hpcrun-opencl.h>			// cl_event, cl_command_queue



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_gpu_blame_shifter
(
	void* dc,
	int metric_id,
	cct_node_t* node,
	int metric_dc
);


void
queue_prologue
(
	cl_command_queue queue
);


void
queue_epilogue
(
	cl_command_queue queue
);


void
kernel_prologue
(
	cl_event event,
	cl_command_queue queue
);


void
kernel_epilogue
(
	cl_event event
);


void
sync_prologue
(
	cl_command_queue queue
);


void
sync_epilogue
(
	cl_command_queue queue
);

#endif 	//blame_shift_opencl_h
