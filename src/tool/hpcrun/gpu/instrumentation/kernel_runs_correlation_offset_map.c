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

#include "kernel_runs_correlation_offset_map.h"



//******************************************************************************
// type declarations
//******************************************************************************

#define kco_insert																					\
  typed_splay_insert(kernel_correlation_offset_map)

#define kco_lookup																					\
  typed_splay_lookup(kernel_correlation_offset_map)

#define kco_delete																					\
  typed_splay_delete(kernel_correlation_offset_map)

#define kco_forall																					\
  typed_splay_forall(kernel_correlation_offset_map)

#define kco_count																						\
  typed_splay_count(kernel_correlation_offset_map)

#define kco_alloc(free_list)																\
  typed_splay_alloc(free_list, kernel_correlation_offset_map_t)

#define kco_free(free_list, node)														\
  typed_splay_free(free_list, node)

typed_splay_impl(kernel_correlation_offset_map);



//******************************************************************************
// local data
//******************************************************************************

static kernel_correlation_offset_map_t *kernel_correlation_offset_map_root = NULL;
static kernel_correlation_offset_map_t *kernel_correlation_offset_map_free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static kernel_correlation_offset_map_t *
kernel_data_alloc()
{
  return kco_alloc(&kernel_correlation_offset_map_free_list);
}


static kernel_correlation_offset_map_t *
kernel_data_new
(
	uint64_t GTPinKernelExec_id,
	kernel_runs_correlation_offset *data
)
{
  kernel_correlation_offset_map_t *e = kernel_data_alloc();
  memset(e, 0, sizeof(kernel_correlation_offset_map_t)); 
  e->GTPinKernelExec_id = GTPinKernelExec_id;
  e->head = data;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

kernel_correlation_offset_map_t*
kernel_correlation_offset_map_lookup1
(
	uint64_t GTPinKernelExec_id
)
{
  kernel_correlation_offset_map_t *result = kco_lookup(&kernel_correlation_offset_map_root, GTPinKernelExec_id);
	return result;
}


void
kernel_correlation_offset_map_insert1
(
	uint64_t GTPinKernelExec_id,
	kernel_runs_correlation_offset *data
)
{
	if (kco_lookup(&kernel_correlation_offset_map_root, GTPinKernelExec_id)) {
		assert(0);	// entry for a given key should be inserted only once
	} else {
		kernel_correlation_offset_map_t *entry = kernel_data_new(GTPinKernelExec_id, data);
		kco_insert(&kernel_correlation_offset_map_root, entry);	
	}
}


void
kernel_correlation_offset_map_delete1
(
	uint64_t GTPinKernelExec_id
)
{
	kernel_correlation_offset_map_t *node = kco_delete(&kernel_correlation_offset_map_root, GTPinKernelExec_id);
	kco_free(&kernel_correlation_offset_map_free_list, node);
}

