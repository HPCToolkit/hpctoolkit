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

#include "opencl-queue-map.h"

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>



//******************************************************************************
// type declarations
//******************************************************************************

#define queue_insert \
  typed_splay_insert(queue_node)

#define queue_lookup \
  typed_splay_lookup(queue_node)

#define queue_delete \
  typed_splay_delete(queue_node)

#define queue_forall \
  typed_splay_forall(queue_node)

#define queue_count \
  typed_splay_count(queue_node)

#define queue_alloc(free_list) \
  typed_splay_alloc(free_list, queue_map_entry_t)

#define queue_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(queue_node) queue_map_entry_t

typedef struct typed_splay_node(queue_node) {
  struct typed_splay_node(queue_node) *left;
  struct typed_splay_node(queue_node) *right;
  uint64_t queue_id; // key

  queue_node_t *node;
} typed_splay_node(queue_node);

typed_splay_impl(queue_node);



//******************************************************************************
// local data
//******************************************************************************

static queue_map_entry_t *queue_map_root = NULL;
static queue_map_entry_t *queue_map_free_list = NULL;

static spinlock_t queue_map_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

static queue_map_entry_t *
queue_node_alloc()
{
  return queue_alloc(&queue_map_free_list);
}


static queue_map_entry_t *
queue_node_new
(
 uint64_t queue_id,
 queue_node_t *node
)
{
  queue_map_entry_t *e = queue_node_alloc();
  e->queue_id = queue_id;
  e->node = node;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

queue_map_entry_t*
queue_map_lookup
(
 uint64_t queue_id
)
{
  spinlock_lock(&queue_map_lock);
  queue_map_entry_t *result = queue_lookup(&queue_map_root, queue_id);
  spinlock_unlock(&queue_map_lock);
  return result;
}


void
queue_map_insert
(
 uint64_t queue_id,
 queue_node_t *node
)
{
  if (queue_lookup(&queue_map_root, queue_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    spinlock_lock(&queue_map_lock);
    queue_map_entry_t *entry = queue_node_new(queue_id, node);
    queue_insert(&queue_map_root, entry);  
    spinlock_unlock(&queue_map_lock);
  }
}


void
queue_map_delete
(
 uint64_t queue_id
)
{
	spinlock_lock(&queue_map_lock);
  queue_map_entry_t *node = queue_delete(&queue_map_root, queue_id);
  queue_free(&queue_map_free_list, node);
	spinlock_unlock(&queue_map_lock);
}


queue_node_t*
queue_map_entry_queue_node_get
(
 queue_map_entry_t *entry
)
{
  return entry->node;
}

