//*****************************************************************************
// global includes
//*****************************************************************************

#include <assert.h>
#include <string.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-correlation-id-map.h"
#include "gpu-splay-allocator.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif


#define st_insert				\
  typed_splay_insert(correlation_id)

#define st_lookup				\
  typed_splay_lookup(correlation_id)

#define st_delete				\
  typed_splay_delete(correlation_id)

#define st_forall				\
  typed_splay_forall(correlation_id)

#define st_count				\
  typed_splay_count(correlation_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_correlation_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(correlation_id) gpu_correlation_id_map_entry_t

typedef struct typed_splay_node(correlation_id) {
  struct typed_splay_node(correlation_id) *left;
  struct typed_splay_node(correlation_id) *right;
  uint32_t correlation_id; // key

  uint64_t external_id;
  uint32_t device_id;
  uint64_t start;
  uint64_t end;
} typed_splay_node(correlation_id); 





//******************************************************************************
// local data
//******************************************************************************

static gpu_correlation_id_map_entry_t *map_root = NULL;

static gpu_correlation_id_map_entry_t *free_list = NULL;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(correlation_id)


static gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_entry_new
(
 uint32_t correlation_id, 
 uint64_t external_id
)
{
  gpu_correlation_id_map_entry_t *e = gpu_correlation_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_correlation_id_map_entry_t)); 

  e->correlation_id = correlation_id;
  e->external_id = external_id;

  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_lookup
(
 uint32_t correlation_id
)
{
  gpu_correlation_id_map_entry_t *result = st_lookup(&map_root, correlation_id);

  TMSG(DEFER_CTXT, "correlation_id map lookup: id=0x%lx (record %p)", 
       correlation_id, result);

  return result;
}


void
gpu_correlation_id_map_insert
(
 uint32_t correlation_id, 
 uint64_t external_id
)
{
  if (st_lookup(&map_root, correlation_id)) { 
    // fatal error: correlation_id already present; a
    // correlation should be inserted only once.
    assert(0);
  } else {
    gpu_correlation_id_map_entry_t *entry = 
      gpu_correlation_id_map_entry_new(correlation_id, external_id);

    st_insert(&map_root, entry);

    PRINT("correlation_id_map insert: correlation_id=0x%lx external_id=%ld (entry=%p)\n", 
	  correlation_id, external_id, entry);
  }
}


// TODO(Keren): remove
void
gpu_correlation_id_map_external_id_replace
(
 uint32_t correlation_id, 
 uint64_t external_id
)
{
  TMSG(DEFER_CTXT, "correlation_id map replace: id=0x%lx");

  gpu_correlation_id_map_entry_t *entry = st_lookup(&map_root, correlation_id);
  if (entry) {
    entry->external_id = external_id;
  }
}


void
gpu_correlation_id_map_delete
(
 uint32_t correlation_id
)
{
  gpu_correlation_id_map_entry_t *node = st_delete(&map_root, correlation_id);
  st_free(free_list, node);
}


void
gpu_correlation_id_map_kernel_update
(
 uint32_t correlation_id,
 uint32_t device_id,
 uint64_t start,
 uint64_t end
)
{
  TMSG(DEFER_CTXT, "correlation_id map replace: id=0x%lx");

  gpu_correlation_id_map_entry_t *entry = st_lookup(&map_root, correlation_id);
  if (entry) {
    entry->device_id = device_id;
    entry->start = start;
    entry->end = end;
  }
}


uint64_t
gpu_correlation_id_map_entry_external_id_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->external_id;
}


uint64_t
gpu_correlation_id_map_entry_start_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->start;
}


uint64_t
gpu_correlation_id_map_entry_end_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->end;
}


uint32_t
gpu_correlation_id_map_entry_device_id_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->device_id;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
gpu_correlation_id_map_count
(
 void
)
{
  return st_count(map_root);
}
