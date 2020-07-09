
//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>
//#include <gtpin.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct KernelData {
	uint64_t kernel_cct_correlation_id;
  char *name;
  uint32_t call_count;
	uint32_t loadmap_module_id;
	//block_map_t *block_map_root;
} KernelData;


#undef typed_splay_node
#define typed_splay_node(kernel_data_map) kernel_data_map_t


typedef struct typed_splay_node(kernel_data_map) {
  struct typed_splay_node(kernel_data_map) *left;
  struct typed_splay_node(kernel_data_map) *right;
  uint64_t GTPinKernel_id; // key

	KernelData data;
}typed_splay_node(kernel_data_map);



//******************************************************************************
// interface operations
//******************************************************************************

kernel_data_map_t*
kernel_data_map_lookup1
(
	uint64_t
);


void
kernel_data_map_insert1
(
	uint64_t,
	KernelData
);


void
kernel_data_map_delete1
(
	uint64_t
);

