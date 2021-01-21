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

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>
#include "blame-shift-datastructure.h"



//******************************************************************************
// type declarations
//******************************************************************************

#define stream_insert \
  typed_splay_insert(stream_node)

#define stream_lookup \
  typed_splay_lookup(stream_node)

#define stream_delete \
  typed_splay_delete(stream_node)

#define stream_forall \
  typed_splay_forall(stream_node)

#define stream_count \
  typed_splay_count(stream_node)

#define stream_alloc(free_list) \
  typed_splay_alloc(free_list, stream_map_entry_t)

#define stream_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(stream_node) stream_map_entry_t

typedef struct typed_splay_node(stream_node) {
  struct typed_splay_node(stream_node) *left;
  struct typed_splay_node(stream_node) *right;
  uint64_t stream_id; // key

  stream_node_t node;
} typed_splay_node(stream_node);

typed_splay_impl(stream_node);



//******************************************************************************
// local data
//******************************************************************************

static stream_map_entry_t *stream_map_root = NULL;
static stream_map_entry_t *stream_map_free_list = NULL;

static spinlock_t stream_map_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

static stream_map_entry_t *
stream_node_alloc()
{
  return stream_alloc(&stream_map_free_list);
}


static stream_map_entry_t *
stream_node_new
(
 uint64_t stream_id,
 stream_node_t node
)
{
  stream_map_entry_t *e = stream_node_alloc();
  e->stream_id = stream_id;
  e->node = node;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

stream_map_entry_t*
stream_map_lookup
(
 uint64_t stream_id
)
{
  spinlock_lock(&stream_map_lock);
  stream_map_entry_t *result = stream_lookup(&stream_map_root, stream_id);
  spinlock_unlock(&stream_map_lock);
  return result;
}


void
stream_map_insert
(
 uint64_t stream_id,
 stream_node_t node
)
{
  if (stream_lookup(&stream_map_root, stream_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    spinlock_lock(&stream_map_lock);
    stream_map_entry_t *entry = stream_node_new(stream_id, node);
    stream_insert(&stream_map_root, entry);  
    spinlock_unlock(&stream_map_lock);
  }
}


void
stream_map_delete
(
 uint64_t stream_id
)
{
  stream_map_entry_t *node = stream_delete(&stream_map_root, stream_id);
  stream_free(&stream_map_free_list, node);
}


stream_node_t
stream_map_entry_stream_node_get
(
 stream_map_entry_t *entry
)
{
  return entry->node;
}

