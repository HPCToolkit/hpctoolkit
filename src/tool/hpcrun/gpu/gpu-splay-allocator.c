//******************************************************************************
// system includes
//******************************************************************************

#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-splay-allocator.h"



//******************************************************************************
// macros
//******************************************************************************

#define NEXT(node) node->left



//******************************************************************************
// interface functions
//******************************************************************************

splay_uint64_node_t *
splay_uint64_alloc_helper
(
 splay_uint64_node_t **free_list, 
 size_t size
)
{
  splay_uint64_node_t *first = *free_list; 

  if (first) { 
    *free_list = NEXT(first);
  } else {
    first = (splay_uint64_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


void
splay_uint64_free_helper
(
 splay_uint64_node_t **free_list, 
 splay_uint64_node_t *node 
)
{
  NEXT(node) = *free_list;
  *free_list = node;
}
