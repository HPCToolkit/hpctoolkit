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

#include "cupti-function-id-map.h"

#define CUPTI_FUNCTION_ID_DEBUG 0

#if CUPTI_FUNCTION_ID_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_function_id_map_entry_s {
  uint32_t function_id;  // cupti function id
  ip_normalized_t ip_norm;
  struct cupti_function_id_map_entry_s *left;
  struct cupti_function_id_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_function_id_map_entry_t *cupti_function_id_map_root = NULL;
static spinlock_t cupti_function_id_map_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_function_id_map_entry_t *
cupti_function_id_map_entry_new(uint32_t function_id, ip_normalized_t ip_norm)
{
  cupti_function_id_map_entry_t *e;
  e = (cupti_function_id_map_entry_t *)hpcrun_malloc_safe(sizeof(cupti_function_id_map_entry_t));
  e->function_id = function_id;
  e->ip_norm = ip_norm;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_function_id_map_entry_t *
cupti_function_id_map_splay(cupti_function_id_map_entry_t *root, uint32_t key)
{
  REGULAR_SPLAY_TREE(cupti_function_id_map_entry_s, root, key, function_id, left, right);
  return root;
}


static void
__attribute__ ((unused))
cupti_function_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "function_id %d: delete", cupti_function_id_map_root->function_id);

  if (cupti_function_id_map_root->left == NULL) {
    cupti_function_id_map_root = cupti_function_id_map_root->right;
  } else {
    cupti_function_id_map_root->left = 
      cupti_function_id_map_splay(cupti_function_id_map_root->left, 
        cupti_function_id_map_root->function_id);
    cupti_function_id_map_root->left->right = cupti_function_id_map_root->right;
    cupti_function_id_map_root = cupti_function_id_map_root->left;
  }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_function_id_map_entry_t *
cupti_function_id_map_lookup(uint32_t function_id)
{
  cupti_function_id_map_entry_t *result = NULL;
  spinlock_lock(&cupti_function_id_map_lock);

  cupti_function_id_map_root = cupti_function_id_map_splay(cupti_function_id_map_root, function_id);
  if (cupti_function_id_map_root && cupti_function_id_map_root->function_id == function_id) {
    result = cupti_function_id_map_root;
  }

  spinlock_unlock(&cupti_function_id_map_lock);

  TMSG(DEFER_CTXT, "function_id map lookup: id=0x%lx (record %p)", function_id, result);
  return result;
}


void
cupti_function_id_map_insert(uint32_t function_id, ip_normalized_t ip_norm)
{
  cupti_function_id_map_entry_t *entry = cupti_function_id_map_entry_new(function_id, ip_norm);

  TMSG(DEFER_CTXT, "function_id map insert: id=0x%lx (record %p)", function_id, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&cupti_function_id_map_lock);

  if (cupti_function_id_map_root != NULL) {
    cupti_function_id_map_root = 
      cupti_function_id_map_splay(cupti_function_id_map_root, function_id);

    if (function_id < cupti_function_id_map_root->function_id) {
      entry->left = cupti_function_id_map_root->left;
      entry->right = cupti_function_id_map_root;
      cupti_function_id_map_root->left = NULL;
    } else if (function_id > cupti_function_id_map_root->function_id) {
      entry->left = cupti_function_id_map_root;
      entry->right = cupti_function_id_map_root->right;
      cupti_function_id_map_root->right = NULL;
    } else {
      // function_id already present: fatal error since a function_id 
      //   should only be inserted once 
      PRINT("function_id duplicated %d\n", function_id);
      assert(0);
    }
  }
  cupti_function_id_map_root = entry;

  spinlock_unlock(&cupti_function_id_map_lock);
}


ip_normalized_t
cupti_function_id_map_entry_ip_norm_get(cupti_function_id_map_entry_t *entry)
{
  return entry->ip_norm;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_function_id_map_count_helper(cupti_function_id_map_entry_t *entry) 
{
  if (entry) {
    int left = cupti_function_id_map_count_helper(entry->left);
    int right = cupti_function_id_map_count_helper(entry->right);
    return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_function_id_map_count() 
{
  return cupti_function_id_map_count_helper(cupti_function_id_map_root);
}
