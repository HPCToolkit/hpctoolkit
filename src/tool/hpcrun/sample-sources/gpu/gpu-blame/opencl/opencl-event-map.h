#ifndef gpu_blame_opencl_event_map_h_
#define gpu_blame_opencl_event_map_h_

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>                   // cct_node_t
#include <lib/prof-lean/hpcrun-opencl.h>      // cl_event
#include <lib/prof-lean/stdatomic.h>          // _Atomic



//******************************************************************************
// type declarations
//******************************************************************************

// Each event_node_t maintains information about an asynchronous opencl activity (kernel or memcpy)
// Note that there are some implicit memory transfers that dont have events bound to them
typedef struct event_node_t {
  cl_event event;

  // start and end times of event_start and event_end
  double event_start_time;
  double event_end_time;

  // CCT node of the CPU thread that launched this activity
  cct_node_t *launcher_cct;

  double cpu_idle_blame;

  _Atomic (struct event_node_t*) next;
} event_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct event_map_entry_t event_map_entry_t;

event_map_entry_t*
event_map_lookup
(
 uint64_t event_id
);


void
event_map_insert
(
 uint64_t event_id,
 event_node_t *event_node
);


void
event_map_delete
(
 uint64_t event_id
);


event_node_t*
event_map_entry_event_node_get
(
 event_map_entry_t *entry
);

#endif		// gpu_blame_opencl_event_map_h_
