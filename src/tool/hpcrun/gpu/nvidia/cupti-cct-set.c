#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-cct-set.h"
#include "../gpu-splay-allocator.h"


//******************************************************************************
// macros
//******************************************************************************

#define st_insert				\
  typed_splay_insert(cct)

#define st_lookup				\
  typed_splay_lookup(cct)

#define st_delete				\
  typed_splay_delete(cct)

#define st_forall				\
  typed_splay_forall(cct)

#define st_count				\
  typed_splay_count(cct)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(cct))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(cct) cupti_cct_set_entry_t 

struct cupti_cct_set_entry_s {
  struct cupti_cct_set_entry_s *left;
  struct cupti_cct_set_entry_s *right;
  uint64_t cct;
};

static cupti_cct_set_entry_t *map_root = NULL;
static cupti_cct_set_entry_t *free_list = NULL;


typed_splay_impl(cct)


static void
flush_fn_helper
(
 cupti_cct_set_entry_t *entry,
 splay_visit_t visit_type,
 void *args
)
{
  if (visit_type == splay_postorder_visit) {
    st_free(&free_list, entry);
  }
}


cupti_cct_set_entry_t *
cupti_cct_set_lookup
(
 cct_node_t *cct
)
{
  cupti_cct_set_entry_t *result = st_lookup(&map_root, (uint64_t)cct);

  return result;
}


void
cupti_cct_set_insert
(
 cct_node_t *cct
)
{
  cupti_cct_set_entry_t *entry = st_lookup(&map_root, (uint64_t)cct);

  if (entry == NULL) {
    entry = st_alloc(&free_list);
    entry->cct = (uint64_t)cct;
    st_insert(&map_root, entry);
  }
}


void
cupti_cct_set_clear
(
)
{
  st_forall(map_root, splay_allorder, flush_fn_helper, NULL);
  map_root = NULL;
}
