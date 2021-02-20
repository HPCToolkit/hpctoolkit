#ifndef gpu_blame_kernel_map_h_
#define gpu_blame_kernel_map_h_

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>                   // cct_node_t
#include <lib/prof-lean/stdatomic.h>          // _Atomic



//******************************************************************************
// type declarations
//******************************************************************************

// Each kernel_node_t maintains information about an asynchronous kernel activity
typedef struct kernel_node_t {
  // we save the id for the kernel inside the node so that ids of processed kernel nodes can be returned on sync_epilogue
  uint64_t kernel_id;

  // start and end times of kernel_start and kernel_end
  unsigned long kernel_start_time;
  unsigned long kernel_end_time;

  // CCT node of the CPU thread that launched this activity
  cct_node_t *launcher_cct;

  double cpu_idle_blame;

  _Atomic (struct kernel_node_t*) next;
} kernel_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct kernel_map_entry_t kernel_map_entry_t;

kernel_map_entry_t*
kernel_map_lookup
(
 uint64_t kernel_id
);


void
kernel_map_insert
(
 uint64_t kernel_id,
 kernel_node_t *kernel_node
);


void
kernel_map_delete
(
 uint64_t kernel_id
);


kernel_node_t*
kernel_map_entry_kernel_node_get
(
 kernel_map_entry_t *entry
);

#endif		// gpu_blame_kernel_map_h_
