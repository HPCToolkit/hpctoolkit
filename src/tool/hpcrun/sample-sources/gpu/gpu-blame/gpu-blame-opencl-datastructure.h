#ifndef blame_shift_datastructure_h
#define blame_shift_datastructure_h

//******************************************************************************
// system includes
//******************************************************************************


//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>										// cct_node_t
#include <lib/prof-lean/hpcrun-opencl.h>



//******************************************************************************
// type declarations
//******************************************************************************

// Each event_list_node_t maintains information about an asynchronous opencl activity (kernel or memcpy)
// Note that there are some implicit memory transfers that dont have events bound to them
typedef struct event_list_node_t {
	// cudaEvent inserted immediately before and after the activity
	cl_event event;

	// start and end times of event_start and event_end
	unsigned long event_start_time;
	unsigned long event_end_time;

	// CCT node of the CPU thread that launched this activity
	cct_node_t *launcher_cct;
	// CCT node of the queue
	cct_node_t *queue_launcher_cct;

	// Outstanding threads that need to examine this activity
	uint32_t ref_count;

	// our internal splay tree id for the corresponding cudaStream for this activity
	uint64_t queue_id;
	union {
		struct event_list_node_t *next;
		struct event_list_node_t *next_free_node;
	};
} event_list_node_t;


// Per GPU queue information
typedef struct queue_node_t {
	// hpcrun profiling and tracing infp
	struct core_profile_trace_data_t *st;

	// I think we need to maintain head and tail pointers for events here
	struct event_list_node_t *event_list_head;
	struct event_list_node_t *event_list_tail;

	// pointer to the next queue which has activities pending
	struct queue_node_t *next_unfinished_queue;

	/*
	// used to remove from hpcrun cleanup list if queue is explicitly destroyed
	hpcrun_aux_cleanup_t * aux_cleanup_info;
	// IDLE NODE persistent id for this queue
	int32_t idle_node_id;
	*/
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

#endif		//blame_shift_datastructure_h
