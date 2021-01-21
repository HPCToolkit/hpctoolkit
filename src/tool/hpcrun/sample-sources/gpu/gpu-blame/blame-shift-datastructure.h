#ifndef blame_shift_datastructure_h
#define blame_shift_datastructure_h

//******************************************************************************
// system includes
//******************************************************************************


//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-opencl.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct stream_node_t {
  uint64_t kernel_key;
  cl_event event;
} stream_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct stream_map_entry_t stream_map_entry_t;

stream_map_entry_t*
stream_map_lookup
(
 uint64_t stream_id
);


void
stream_map_insert
(
 uint64_t stream_id,
 stream_node_t stream_node
);


void
stream_map_delete
(
 uint64_t stream_id
);


stream_node_t
stream_map_entry_stream_node_get
(
 stream_map_entry_t *entry
);

#endif		//blame_shift_datastructure_h
