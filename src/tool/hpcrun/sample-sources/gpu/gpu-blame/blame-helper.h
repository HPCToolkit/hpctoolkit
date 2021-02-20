#ifndef blame_shift_helper_h
#define blame_shift_helper_h

//******************************************************************************
// local includes
//******************************************************************************

#include "blame-kernel-map.h"		// kernel_node_t, queue_node_t



//******************************************************************************
// interface operations
//******************************************************************************

void
calculate_blame_for_active_kernels
(
 kernel_node_t *kernel_list,
 unsigned long sync_start,
 unsigned long sync_end
);

#endif 	//blame_shift_helper_h
