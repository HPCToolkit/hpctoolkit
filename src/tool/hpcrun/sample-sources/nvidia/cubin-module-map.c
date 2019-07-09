/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/crypto-hash.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cubin-module-map.h"

#define CUBIN_MODULE_MAP_DEBUG 0

#if CUBIN_MODULE_MAP_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cubin_module_map_entry_s {
  const void *module;
  uint32_t hpctoolkit_module_id;
  Elf_SymbolVector *elf_vector;
  unsigned char *hash;
  struct cubin_module_map_entry_s *left;
  struct cubin_module_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cubin_module_map_entry_t *cubin_module_map_root = NULL;
static spinlock_t cubin_module_map_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cubin_module_map_entry_t *
cubin_module_map_entry_new(const void *module, Elf_SymbolVector *vector)
{
  cubin_module_map_entry_t *e;
  e = (cubin_module_map_entry_t *)hpcrun_malloc(sizeof(cubin_module_map_entry_t));
  e->module = module;
  e->left = NULL;
  e->right = NULL;
  e->elf_vector = vector;

  return e;
}


static cubin_module_map_entry_t *
cubin_module_map_splay(cubin_module_map_entry_t *root, const void *key)
{
  REGULAR_SPLAY_TREE(cubin_module_map_entry_s, root, key, module, left, right);
  return root;
}


static void
cubin_module_map_delete_root()
{
  TMSG(DEFER_CTXT, "module %p: delete", cubin_module_map_root->module);

  if (cubin_module_map_root->left == NULL) {
    cubin_module_map_root = cubin_module_map_root->right;
  } else {
    cubin_module_map_root->left = 
      cubin_module_map_splay(cubin_module_map_root->left, 
			   cubin_module_map_root->module);
    cubin_module_map_root->left->right = cubin_module_map_root->right;
    cubin_module_map_root = cubin_module_map_root->left;
  }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_module_map_entry_t *
cubin_module_map_lookup(const void *module)
{
  cubin_module_map_entry_t *result = NULL;
  spinlock_lock(&cubin_module_map_lock);

  cubin_module_map_root = cubin_module_map_splay(cubin_module_map_root, module);
  if (cubin_module_map_root && cubin_module_map_root->module == module) {
    result = cubin_module_map_root;
  }

  spinlock_unlock(&cubin_module_map_lock);

  TMSG(DEFER_CTXT, "cubin_module map lookup: module=0x%lx (record %p)", module, result);
  return result;
}


void
cubin_module_map_insert(const void *module, uint32_t hpctoolkit_module_id, Elf_SymbolVector *vector)
{
  spinlock_lock(&cubin_module_map_lock);

  if (cubin_module_map_root != NULL) {
    cubin_module_map_root = 
      cubin_module_map_splay(cubin_module_map_root, module);

    if (module < cubin_module_map_root->module) {
      cubin_module_map_entry_t *entry = cubin_module_map_entry_new(module, vector);
      TMSG(DEFER_CTXT, "cubin_module map insert: module=0x%lx (record %p)", module, entry);
      entry->left = entry->right = NULL;
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      entry->left = cubin_module_map_root->left;
      entry->right = cubin_module_map_root;
      cubin_module_map_root->left = NULL;
      cubin_module_map_root = entry;
    } else if (module > cubin_module_map_root->module) {
      cubin_module_map_entry_t *entry = cubin_module_map_entry_new(module, vector);
      TMSG(DEFER_CTXT, "cubin_module map insert: module=0x%lx (record %p)", module, entry);
      entry->left = entry->right = NULL;
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      entry->left = cubin_module_map_root;
      entry->right = cubin_module_map_root->right;
      cubin_module_map_root->right = NULL;
      cubin_module_map_root = entry;
    } else {
      // cubin_module already present
    }
  } else {
      cubin_module_map_entry_t *entry = cubin_module_map_entry_new(module, vector);
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      cubin_module_map_root = entry;
  }

  spinlock_unlock(&cubin_module_map_lock);
}


void
cubin_module_map_insert_hash(const void *module, unsigned char *hash)
{
  spinlock_lock(&cubin_module_map_lock);

  if (cubin_module_map_root != NULL) {
    cubin_module_map_root = 
      cubin_module_map_splay(cubin_module_map_root, module);

    if (module == cubin_module_map_root->module) {
      // cubin_module already present
      cubin_module_map_root->hash = hash;
    }
  }

  spinlock_unlock(&cubin_module_map_lock);
}


void
cubin_module_map_delete
(
 const void *module
)
{
  spinlock_lock(&cubin_module_map_lock);

  cubin_module_map_root = cubin_module_map_splay(cubin_module_map_root, module);
  if (cubin_module_map_root && cubin_module_map_root->module == module) {
    cubin_module_map_delete_root();
  }

  spinlock_unlock(&cubin_module_map_lock);
}


uint32_t
cubin_module_map_entry_hpctoolkit_id_get(cubin_module_map_entry_t *entry)
{
  return entry->hpctoolkit_module_id;
}


Elf_SymbolVector *
cubin_module_map_entry_elf_vector_get(cubin_module_map_entry_t *entry)
{
  return entry->elf_vector;
}


unsigned char *
cubin_module_map_entry_hash_get(cubin_module_map_entry_t *entry, unsigned int *len)
{
  *len = crypto_hash_length();
  return entry->hash;
}


//--------------------------------------------------------------------------
// Transform a <module, function_id, offset> tuple to a pc address by
// looking up elf symbols inside a cubin
//--------------------------------------------------------------------------
ip_normalized_t
cubin_module_transform(const void *module, uint32_t function_id, int64_t offset)
{
  cubin_module_map_entry_t *entry = cubin_module_map_lookup(module);
  ip_normalized_t ip;
  if (entry != NULL) {
    uint32_t hpctoolkit_module_id = cubin_module_map_entry_hpctoolkit_id_get(entry);
    PRINT("get hpctoolkit_module_id %d\n", hpctoolkit_module_id);
    const Elf_SymbolVector *vector = cubin_module_map_entry_elf_vector_get(entry);
    ip.lm_id = (uint16_t)hpctoolkit_module_id;
    ip.lm_ip = (uintptr_t)(vector->symbols[function_id] + offset);
  }
  return ip;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cubin_module_map_count_helper(cubin_module_map_entry_t *entry) 
{
  if (entry) {
     int left = cubin_module_map_count_helper(entry->left);
     int right = cubin_module_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cubin_module_map_count() 
{
  return cubin_module_map_count_helper(cubin_module_map_root);
}

