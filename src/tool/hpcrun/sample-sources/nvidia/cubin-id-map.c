/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cubin-id-map.h"

#define CUPTI_CUBIN_ID_MAP_DEBUG 0

#if CUPTI_CUBIN_ID_MAP_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cubin_id_map_entry_s {
  uint32_t cubin_id;
  uint32_t hpctoolkit_module_id;
  Elf_SymbolVector *elf_vector;
  struct cubin_id_map_entry_s *left;
  struct cubin_id_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cubin_id_map_entry_t *cubin_id_map_root = NULL;
static spinlock_t cubin_id_map_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cubin_id_map_entry_t *
cubin_id_map_entry_new(uint32_t cubin_id, Elf_SymbolVector *vector)
{
  cubin_id_map_entry_t *e;
  e = (cubin_id_map_entry_t *)hpcrun_malloc_safe(sizeof(cubin_id_map_entry_t));
  e->cubin_id = cubin_id;
  e->left = NULL;
  e->right = NULL;
  e->elf_vector = vector;

  return e;
}


static cubin_id_map_entry_t *
cubin_id_map_splay(cubin_id_map_entry_t *root, uint32_t key)
{
  REGULAR_SPLAY_TREE(cubin_id_map_entry_s, root, key, cubin_id, left, right);
  return root;
}


static void
cubin_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "cubin_id %d: delete", cubin_id_map_root->cubin_id);

  if (cubin_id_map_root->left == NULL) {
    cubin_id_map_root = cubin_id_map_root->right;
  } else {
    cubin_id_map_root->left = 
      cubin_id_map_splay(cubin_id_map_root->left, 
			   cubin_id_map_root->cubin_id);
    cubin_id_map_root->left->right = cubin_id_map_root->right;
    cubin_id_map_root = cubin_id_map_root->left;
  }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_id_map_entry_t *
cubin_id_map_lookup(uint32_t id)
{
  cubin_id_map_entry_t *result = NULL;
  spinlock_lock(&cubin_id_map_lock);

  cubin_id_map_root = cubin_id_map_splay(cubin_id_map_root, id);
  if (cubin_id_map_root && cubin_id_map_root->cubin_id == id) {
    result = cubin_id_map_root;
  }

  spinlock_unlock(&cubin_id_map_lock);

  TMSG(DEFER_CTXT, "cubin_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cubin_id_map_insert(uint32_t cubin_id, uint32_t hpctoolkit_module_id, Elf_SymbolVector *vector)
{
  spinlock_lock(&cubin_id_map_lock);

  if (cubin_id_map_root != NULL) {
    cubin_id_map_root = 
      cubin_id_map_splay(cubin_id_map_root, cubin_id);

    if (cubin_id < cubin_id_map_root->cubin_id) {
      cubin_id_map_entry_t *entry = cubin_id_map_entry_new(cubin_id, vector);
      TMSG(DEFER_CTXT, "cubin_id map insert: id=0x%lx (record %p)", cubin_id, entry);
      entry->left = entry->right = NULL;
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      entry->left = cubin_id_map_root->left;
      entry->right = cubin_id_map_root;
      cubin_id_map_root->left = NULL;
      cubin_id_map_root = entry;
    } else if (cubin_id > cubin_id_map_root->cubin_id) {
      cubin_id_map_entry_t *entry = cubin_id_map_entry_new(cubin_id, vector);
      TMSG(DEFER_CTXT, "cubin_id map insert: id=0x%lx (record %p)", cubin_id, entry);
      entry->left = entry->right = NULL;
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      entry->left = cubin_id_map_root;
      entry->right = cubin_id_map_root->right;
      cubin_id_map_root->right = NULL;
      cubin_id_map_root = entry;
    } else {
      // cubin_id already present
    }
  } else {
      cubin_id_map_entry_t *entry = cubin_id_map_entry_new(cubin_id, vector);
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      cubin_id_map_root = entry;
  }

  spinlock_unlock(&cubin_id_map_lock);
}


void
cubin_id_map_delete
(
 uint32_t cubin_id
)
{
  cubin_id_map_root = cubin_id_map_splay(cubin_id_map_root, cubin_id);
  if (cubin_id_map_root && cubin_id_map_root->cubin_id == cubin_id) {
    cubin_id_map_delete_root();
  }
}


uint32_t
cubin_id_map_entry_hpctoolkit_id_get(cubin_id_map_entry_t *entry)
{
  return entry->hpctoolkit_module_id;
}


Elf_SymbolVector *
cubin_id_map_entry_elf_vector_get(cubin_id_map_entry_t *entry)
{
  return entry->elf_vector;
}


//--------------------------------------------------------------------------
// Transform a <cubin_id, function_id, offset> tuple to a pc address by
// looking up elf symbols inside a cubin
//--------------------------------------------------------------------------
ip_normalized_t
cubin_id_transform(uint32_t cubin_id, uint32_t function_id, int64_t offset)
{
  cubin_id_map_entry_t *entry = cubin_id_map_lookup(cubin_id);
  ip_normalized_t ip;
  PRINT("cubin_id %d\n", cubin_id);
  if (entry != NULL) {
    uint32_t hpctoolkit_module_id = cubin_id_map_entry_hpctoolkit_id_get(entry);
    PRINT("get hpctoolkit_module_id %d\n", hpctoolkit_module_id);
    const Elf_SymbolVector *vector = cubin_id_map_entry_elf_vector_get(entry);
    ip.lm_id = (uint16_t)hpctoolkit_module_id;
    ip.lm_ip = (uintptr_t)(vector->symbols[function_id] + offset);
  }
  return ip;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cubin_id_map_count_helper(cubin_id_map_entry_t *entry) 
{
  if (entry) {
     int left = cubin_id_map_count_helper(entry->left);
     int right = cubin_id_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cubin_id_map_count() 
{
  return cubin_id_map_count_helper(cubin_id_map_root);
}

