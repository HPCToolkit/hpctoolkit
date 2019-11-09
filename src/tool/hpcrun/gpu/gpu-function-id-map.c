//*****************************************************************************
// system includes
//*****************************************************************************

#include <assert.h>
#include <string.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-function-id-map.h"



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
  typed_splay_insert(function_id)

#define st_lookup				\
  typed_splay_lookup(function_id)

#define st_forall				\
  typed_splay_forall(function_id)

#define st_count				\
  typed_splay_count(function_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_function_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(function_id) gpu_function_id_map_entry_t


typedef struct gpu_function_id_map_entry_t {
  struct gpu_function_id_map_entry_t *left;
  struct gpu_function_id_map_entry_t *right;
  uint64_t function_id;  // key - gpu function id

  ip_normalized_t ip_norm;
} gpu_function_id_map_entry_t;



//*****************************************************************************
// global data 
//*****************************************************************************

static gpu_function_id_map_entry_t *map_root = NULL;

static gpu_function_id_map_entry_t *free_list = NULL;

static spinlock_t map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(function_id)


static gpu_function_id_map_entry_t *
gpu_function_id_map_entry_new
(
 uint32_t function_id, 
 ip_normalized_t ip_norm
)
{
  gpu_function_id_map_entry_t *e = (gpu_function_id_map_entry_t *)
    hpcrun_malloc_safe(sizeof(gpu_function_id_map_entry_t));

  memset(e, 0, sizeof(gpu_function_id_map_entry_t));

  e->function_id = function_id;
  e->ip_norm = ip_norm;

  return e;
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

gpu_function_id_map_entry_t *
gpu_function_id_map_lookup
(
 uint32_t function_id
)
{
  spinlock_lock(&map_lock);

  gpu_function_id_map_entry_t *result = st_lookup(&map_root, function_id);

  spinlock_unlock(&map_lock);

  PRINT("function_id map lookup: id=0x%lx (record %p)\n", function_id, result);

  return result;
}


void
gpu_function_id_map_insert
(
 uint32_t function_id, 
 ip_normalized_t ip_norm
)
{
  gpu_function_id_map_entry_t *entry = 
    gpu_function_id_map_entry_new(function_id, ip_norm);

  spinlock_lock(&map_lock);

  if (st_lookup(&map_root, function_id)) {
    // function_id already present: fatal error since a function_id 
    //   should only be inserted once 
    PRINT("function_id duplicated %d\n", function_id);
    assert(0);
  } else {
    st_insert(&map_root, entry);

    PRINT("function_id map insert: id=0x%lx (record %p)\n", function_id, entry);
  }

  spinlock_unlock(&map_lock);
}


ip_normalized_t
gpu_function_id_map_entry_ip_norm_get
(
 gpu_function_id_map_entry_t *entry
)
{
  return entry->ip_norm;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t 
gpu_function_id_map_count() 
{
  uint64_t count;

  spinlock_lock(&map_lock);
  count = st_count(map_root);
  spinlock_unlock(&map_lock);

  return count;
}
