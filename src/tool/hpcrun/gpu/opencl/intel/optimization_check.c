
//******************************************************************************
// local includes
//******************************************************************************

#include "optimization_check.h"



//******************************************************************************
// interface functions
//******************************************************************************

bool
isQueueInInOrderExecutionMode
(
	cl_command_queue_properties properties
)
{
	return (properties && CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
}

