/* Strategy:
 * at each CPU sample, the CPU/host thread will query the completion status of kernel at head each queue in StreamQs datastructure
 * But this approach was needed for Nvidia streams that had in-order kernel execution.
 * Opencl has 2 kernel-execution modes: in-order and out-of-order.
 * Thus we need to rethink our strategy
 * */

/* Scenarios/Action:
 * CPU: inactive, GPU: active			Action: GPU_IDLE_CAUSE for calling_context at GPU is incremented by sampling interval
 * CPU: inactive, GPU: inactive		Action: Need to figure out how to handle this scenario
 * CPU: active, 	GPU: inactive		Action:	CPU_IDLE_CAUSE for calling_context at CPU is incremented by sampling interval
 * CPU: active, 	GPU: active			Action: Nothing to do
 * */



//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "blame-shift-datastructure.h"



//******************************************************************************
// macros
//******************************************************************************



//******************************************************************************
// local data
//******************************************************************************

bool isCPUInactive;	// should this variable be atomic?



//******************************************************************************
// private operations
//******************************************************************************

bool
isCPUBusy() {
	return !isCPUInactive;
}


bool
isGPUBusy() {

}



//******************************************************************************
// interface operations
//******************************************************************************

void
setCPUBusy() {
	isCPUInactive = false;
}


void
setCPUIdle() {
	isCPUInactive = true;
}


void signal_sample_callback() {

	bool cpuBusy = isCPUBusy();
	bool gpuBusy = isGPUBusy();

	// CPU: inactive, GPU: active			Action: GPU_IDLE_CAUSE for calling_context at GPU is incremented by sampling interval
	if (!cpuBusy && gpuBusy) {

	}
	
	// CPU: inactive, GPU: inactive		Action: Need to figure out how to handle this scenario
	if (!cpuBusy && !gpuBusy) {

	}

	// CPU: active, 	GPU: inactive		Action:	CPU_IDLE_CAUSE for calling_context at CPU is incremented by sampling interval
	if (cpuBusy && !gpuBusy) {

	}

	// CPU: active, 	GPU: active			Action: Nothing to do
}

