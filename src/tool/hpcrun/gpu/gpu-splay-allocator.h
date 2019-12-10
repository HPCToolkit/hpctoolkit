#ifndef gpu_splay_allocator_h
#define gpu_splay_allocator_h



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-uint64.h>



//******************************************************************************
// macros
//******************************************************************************

#define typed_splay_alloc(free_list, splay_node_type)		\
  (splay_node_type *) splay_uint64_alloc_helper		\
  ((splay_uint64_node_t **) free_list, sizeof(splay_node_type))	


#define typed_splay_free(free_list, node)			\
  splay_uint64_free_helper					\
  ((splay_uint64_node_t **) free_list,				\
   (splay_uint64_node_t *) node)



//******************************************************************************
// interface functions
//******************************************************************************

splay_uint64_node_t *
splay_uint64_alloc_helper
(
 splay_uint64_node_t **free_list, 
 size_t size
);


void
splay_uint64_free_helper
(
 splay_uint64_node_t **free_list, 
 splay_uint64_node_t *e 
);



#endif
