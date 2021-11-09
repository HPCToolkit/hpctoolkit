#ifndef blame_h
#define blame_h

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>                   // cct_node_t
#include "blame-kernel-cleanup-map.h"         // kernel_id_t



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct cct_node_linkedlist_t {
  _Atomic (struct cct_node_linkedlist_t*) next;
  cct_node_t *node;
} cct_node_linkedlist_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
cct_list_node_free_helper
(
 cct_node_linkedlist_t *node
);


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
 uint64_t queue_id,
 struct timespec sync_start
);


kernel_id_t
sync_epilogue
(
 uint64_t queue_id,
 struct timespec sync_end
);


void
gpu_blame_gpu_utilization_enable
(
 void
);

#endif 	//blame_h
