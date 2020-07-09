
//******************************************************************************
// system includes
//******************************************************************************

#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>

#include "gtpin-instrumentation-kernel-memory-map.h"


//******************************************************************************
// type declarations
//******************************************************************************

#define kmm_insert																					\
  typed_splay_insert(kernel_memory_map)

#define kmm_lookup																					\
  typed_splay_lookup(kernel_memory_map)

#define kmm_delete																					\
  typed_splay_delete(kernel_memory_map)

#define kmm_forall																					\
  typed_splay_forall(kernel_memory_map)

#define kmm_count																						\
  typed_splay_count(kernel_memory_map)

#define kmm_alloc(free_list)																\
  typed_splay_alloc(free_list, kernel_memory_map_t)

#define kmm_free(free_list, node)														\
  typed_splay_free(free_list, node)

typed_splay_impl(kernel_memory_map);



//******************************************************************************
// local data
//******************************************************************************

static kernel_memory_map_t *kernel_memory_map_root = NULL;
static kernel_memory_map_t *kernel_memory_map_free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static kernel_memory_map_t *
kernel_mem_alloc()
{
  return kmm_alloc(&kernel_memory_map_free_list);
}


static kernel_memory_map_t *
kernel_mem_new
(
	uint64_t GTPinKernel_id,
	mem_pair_node *head
)
{
  kernel_memory_map_t *e = kernel_mem_alloc();
  memset(e, 0, sizeof(kernel_memory_map_t)); 
  e->GTPinKernel_id = GTPinKernel_id;
  e->head = head;
  return e;
}




//******************************************************************************
// interface operations
//******************************************************************************

kernel_memory_map_t*
kernel_memory_map_lookup1
(
	uint64_t GTPinKernel_id
)
{
  kernel_memory_map_t *result = kmm_lookup(&kernel_memory_map_root, GTPinKernel_id);
	return result;
}


void
kernel_memory_map_insert1
(
	uint64_t GTPinKernel_id,
	mem_pair_node *head
)
{
	if (kmm_lookup(&kernel_memory_map_root, GTPinKernel_id)) {
		assert(0);	// entry for a given key should be inserted only once
	} else {
		kernel_memory_map_t *entry = kernel_mem_new(GTPinKernel_id, head);
		kmm_insert(&kernel_memory_map_root, entry);	
	}
}


void
kernel_memory_map_delete1
(
	uint64_t GTPinKernel_id
)
{
	kernel_memory_map_t *node = kmm_delete(&kernel_memory_map_root, GTPinKernel_id);
	kmm_free(&kernel_memory_map_free_list, node);
}

