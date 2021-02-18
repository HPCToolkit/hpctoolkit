#ifndef blame_h
#define blame_h

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>                   // cct_node_t
#include "blame-kernel-cleanup-map.h"         // kernel_id_t



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_idle_blame
(
 void* dc,
 int metric_id,
 cct_node_t* node,
 int metric_dc
);


void
queue_prologue
(
 uint64_t queue_id
);


void
queue_epilogue
(
 uint64_t queue_id
);


void
kernel_prologue
(
 uint64_t kernelexec_id
);


void
kernel_epilogue
(
 uint64_t kernelexec_id,
 unsigned long kernel_start,
 unsigned long kernel_end
);


void
sync_prologue
(
 uint64_t queue_id
);


kernel_id_t
sync_epilogue
(
 uint64_t queue_id
);

#endif 	//blame_h
