//******************************************************************************
// system includes
//******************************************************************************

#include <string.h>
#include <assert.h>


//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>

#include "gtpin-instrumentation-kernel-data-map.h"



//******************************************************************************
// type declarations
//******************************************************************************

#define kdm_insert																					\
  typed_splay_insert(kernel_data_map)

#define kdm_lookup																					\
  typed_splay_lookup(kernel_data_map)

#define kdm_delete																					\
  typed_splay_delete(kernel_data_map)

#define kdm_forall																					\
  typed_splay_forall(kernel_data_map)

#define kdm_count																						\
  typed_splay_count(kernel_data_map)

#define kdm_alloc(free_list)																\
  typed_splay_alloc(free_list, kernel_data_map_t)

#define kdm_free(free_list, node)														\
  typed_splay_free(free_list, node)

typed_splay_impl(kernel_data_map);



//******************************************************************************
// local data
//******************************************************************************

static kernel_data_map_t *kernel_data_map_root = NULL;
static kernel_data_map_t *kernel_data_map_free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static kernel_data_map_t *
kernel_data_alloc()
{
  return kdm_alloc(&kernel_data_map_free_list);
}


static kernel_data_map_t *
kernel_data_new
(
	uint64_t GTPinKernel_id,
	KernelData data
)
{
  kernel_data_map_t *e = kernel_data_alloc();
  memset(e, 0, sizeof(kernel_data_map_t)); 
  e->GTPinKernel_id = GTPinKernel_id;
  e->data = data;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

kernel_data_map_t*
kernel_data_map_lookup1
(
	uint64_t GTPinKernel_id
)
{
  kernel_data_map_t *result = kdm_lookup(&kernel_data_map_root, GTPinKernel_id);
	return result;
}


void
kernel_data_map_insert1
(
	uint64_t GTPinKernel_id,
	KernelData data
)
{
	if (kdm_lookup(&kernel_data_map_root, GTPinKernel_id)) {
		assert(0);	// entry for a given key should be inserted only once
	} else {
		kernel_data_map_t *entry = kernel_data_new(GTPinKernel_id, data);
		kdm_insert(&kernel_data_map_root, entry);	
	}
}


void
kernel_data_map_delete1
(
	uint64_t GTPinKernel_id
)
{
	kernel_data_map_t *node = kdm_delete(&kernel_data_map_root, GTPinKernel_id);
	kdm_free(&kernel_data_map_free_list, node);
}

