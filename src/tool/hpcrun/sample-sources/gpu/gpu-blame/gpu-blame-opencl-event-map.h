#ifndef gpu_blame_opencl_event_map_h_
#define gpu_blame_opencl_event_map_h_

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
	cl_event event;

	// start and end times of event_start and event_end
	unsigned long event_start_time;
	unsigned long event_end_time;

	// CCT node of the CPU thread that launched this activity
	cct_node_t *launcher_cct;
	// CCT node of the queue
	cct_node_t *queue_launcher_cct;
	
	bool isDeleted;

	union {
		struct event_list_node_t *next;
		struct event_list_node_t *next_free_node;	// didnt understand this variable's use
	};
} event_list_node_t;



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
 event_list_node_t *event_node
);


void
event_map_delete
(
 uint64_t event_id
);


event_list_node_t*
event_map_entry_event_node_get
(
 event_map_entry_t *entry
);

#endif		// gpu_blame_opencl_event_map_h_
