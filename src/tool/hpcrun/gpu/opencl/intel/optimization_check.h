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

bool
isQueueInInOrderExecutionMode
(
	cl_command_queue_properties properties
);

#endif  // _OPTIMIZATION_CHECK_H_
