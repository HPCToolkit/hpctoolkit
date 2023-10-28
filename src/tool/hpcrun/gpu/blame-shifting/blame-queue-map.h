#ifndef blame_queue_map_h_
#define blame_queue_map_h_

//******************************************************************************
// local includes
//******************************************************************************

#include "blame-kernel-map.h"
#include <hpcrun/cct/cct.h>                   // cct_node_t
#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif



//******************************************************************************
// type declarations
//******************************************************************************

// Per GPU queue information
typedef struct queue_node_t {
  // next pointer is used only for maintaining a list of free nodes
  struct queue_node_t* next;

  // if CPU thread is blocked for queue operations to complete, we use these this variable
  struct timespec cpu_sync_start_time;
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

#endif          // blame_queue_map_h_
