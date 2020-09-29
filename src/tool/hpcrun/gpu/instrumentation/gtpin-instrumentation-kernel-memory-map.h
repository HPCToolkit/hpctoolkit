
//******************************************************************************
// system includes
//******************************************************************************

#include <gtpin.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct mem_pair_node {
	int32_t offset;
	int32_t endOffset;
	GTPinMem mem;
	struct mem_pair_node *next;
} mem_pair_node;


#undef typed_splay_node
#define typed_splay_node(kernel_memory_map) kernel_memory_map_t


typedef struct typed_splay_node(kernel_memory_map) {
  struct typed_splay_node(kernel_memory_map) *left;
  struct typed_splay_node(kernel_memory_map) *right;
  uint64_t GTPinKernel_id; // key

	mem_pair_node *head;

} typed_splay_node(kernel_memory_map);



//******************************************************************************
// interface operations
//******************************************************************************

kernel_memory_map_t*
kernel_memory_map_lookup1
(
	uint64_t
);


void
kernel_memory_map_insert1
(
	uint64_t,
	mem_pair_node *
);


void
kernel_memory_map_delete1
(
	uint64_t
);

