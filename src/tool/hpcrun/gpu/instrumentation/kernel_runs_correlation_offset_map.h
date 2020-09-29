
//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct kernel_runs_correlation_offset {
	int32_t offset;
	uint32_t correlation_id;
	struct kernel_runs_correlation_offset *next;
} kernel_runs_correlation_offset;


#undef typed_splay_node
#define typed_splay_node(kernel_correlation_offset_map) kernel_correlation_offset_map_t


typedef struct typed_splay_node(kernel_correlation_offset_map) {
  struct typed_splay_node(kernel_correlation_offset_map) *left;
  struct typed_splay_node(kernel_correlation_offset_map) *right;
  uint64_t GTPinKernelExec_id; // key

	kernel_runs_correlation_offset *head;
}typed_splay_node(kernel_correlation_offset_map);



//******************************************************************************
// interface operations
//******************************************************************************

kernel_correlation_offset_map_t*
kernel_correlation_offset_map_lookup1
(
	uint64_t
);


void
kernel_correlation_offset_map_insert1
(
	uint64_t,
	kernel_runs_correlation_offset *
);


void
kernel_correlation_offset_map_delete1
(
	uint64_t
);

