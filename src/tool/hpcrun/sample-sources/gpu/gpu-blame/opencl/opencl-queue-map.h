#ifndef gpu_blame_opencl_queue_map_h_
#define gpu_blame_opencl_queue_map_h_

//******************************************************************************
// local includes
//******************************************************************************

#include "opencl-event-map.h"
#include <hpcrun/cct/cct.h>                 // cct_node_t
#include <lib/prof-lean/hpcrun-opencl.h>



//******************************************************************************
// type declarations
//******************************************************************************

// Per GPU queue information
typedef struct queue_node_t {
	// we maintain queue_id here for deleting the queue_node from map
	uint64_t queue_id;

	// hpcrun profiling and tracing infp
	struct core_profile_trace_data_t *st;

#if 0
	struct event_node_t *event_head;
	struct event_node_t *event_tail;
#endif

	// pointer to the next queue which has activities pending
	struct queue_node_t *next;

	// if CPU is block for queue operations to complete, we use these 2 variables
	cct_node_t *cpu_idle_cct;
	struct timespec *cpu_sync_start_time;
} queue_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct queue_map_entry_t queue_map_entry_t;

queue_map_entry_t*
queue_map_lookup
(
 uint64_t queue_id
);


void
queue_map_insert
(
 uint64_t queue_id,
 queue_node_t *queue_node
);


void
queue_map_delete
(
 uint64_t queue_id
);


queue_node_t*
queue_map_entry_queue_node_get
(
 queue_map_entry_t *entry
);

#endif		// gpu_blame_opencl_queue_map_h_
