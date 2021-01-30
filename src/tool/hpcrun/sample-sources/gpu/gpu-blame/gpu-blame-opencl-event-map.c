/* StreamQs data-stucture from Blame-Shifing paper:
 * In theory, we want multiple queues with head and tail pointers. There will be a queue/Linked-List for each stream(CUDA)/queue(OpenCL).
 *
 * For storing the multiple linked-lists, we will use a splay-tree datastructure. We will use a splay-tree instead of queue for the following reasons:
 * 1. Since the count of streams/queues are variable, we need a dynamic datastructure
 * 2. HPCRUN doesnt support C++, which rules out using built-in datastructures(e.g. queues, vectors)
 * 3. Splay-tree implementation is present in HPCToolkit
 *
 * Splay_Node {
 * 	key: 64bit representation of stream/queue variable address.
 * 	val: head pointer of linkedlist for the respective stream/queue
 * }
 * 
 * Each stream/queue will be represented using a Linked-List. Each node in this Linked-List will represent a kernel.
 * LL_Node {
 * 	kernel_key: 64bit representation of kernel variable address. // not needed, maintained for debugging purposes
 * 	event: 			event corresponding to the kernel // TODO: this is OpenCL specific. We might have to change this for maintaining generality
 * }
 * 
 * */



//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-blame-opencl-event-map.h"		// event_list_node_t, queue_node_t

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>



//******************************************************************************
// type declarations
//******************************************************************************

#define event_insert \
  typed_splay_insert(event_node)

#define event_lookup \
  typed_splay_lookup(event_node)

#define event_delete \
  typed_splay_delete(event_node)

#define event_forall \
  typed_splay_forall(event_node)

#define event_count \
  typed_splay_count(event_node)

#define event_alloc(free_list) \
  typed_splay_alloc(free_list, event_map_entry_t)

#define event_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(event_node) event_map_entry_t

typedef struct typed_splay_node(event_node) {
  struct typed_splay_node(event_node) *left;
  struct typed_splay_node(event_node) *right;
  uint64_t event_id; // key

  event_list_node_t *node;
} typed_splay_node(event_node);

typed_splay_impl(event_node);



//******************************************************************************
// local data
//******************************************************************************

static event_map_entry_t *event_map_root = NULL;
static event_map_entry_t *event_map_free_list = NULL;

static spinlock_t event_map_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

static event_map_entry_t *
event_node_alloc()
{
  return event_alloc(&event_map_free_list);
}


static event_map_entry_t *
event_node_new
(
 uint64_t event_id,
 event_list_node_t *node
)
{
  event_map_entry_t *e = event_node_alloc();
  e->event_id = event_id;
  e->node = node;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

event_map_entry_t*
event_map_lookup
(
 uint64_t event_id
)
{
  spinlock_lock(&event_map_lock);
  event_map_entry_t *result = event_lookup(&event_map_root, event_id);
  spinlock_unlock(&event_map_lock);
  return result;
}


void
event_map_insert
(
 uint64_t event_id,
 event_list_node_t *node
)
{
  if (event_lookup(&event_map_root, event_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    spinlock_lock(&event_map_lock);
    event_map_entry_t *entry = event_node_new(event_id, node);
    event_insert(&event_map_root, entry);  
    spinlock_unlock(&event_map_lock);
  }
}


void
event_map_delete
(
 uint64_t event_id
)
{
  event_map_entry_t *node = event_delete(&event_map_root, event_id);
  event_free(&event_map_free_list, node);
}


event_list_node_t*
event_map_entry_event_node_get
(
 event_map_entry_t *entry
)
{
  return entry->node;
}

