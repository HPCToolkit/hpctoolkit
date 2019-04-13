/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cuda-api.h"
#include "cupti-device-id-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_device_id_map_entry_s {
  uint32_t device_id;
  cupti_device_property_t property;
  struct cupti_device_id_map_entry_s *left;
  struct cupti_device_id_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_device_id_map_entry_t *cupti_device_id_map_root = NULL;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_device_id_map_entry_t *
cupti_device_id_map_entry_new(uint32_t device_id)
{
  cupti_device_id_map_entry_t *e;
  e = (cupti_device_id_map_entry_t *)hpcrun_malloc(sizeof(cupti_device_id_map_entry_t));
  e->device_id = device_id;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_device_id_map_entry_t *
cupti_device_id_map_splay(cupti_device_id_map_entry_t *root, uint32_t key)
{
  REGULAR_SPLAY_TREE(cupti_device_id_map_entry_s, root, key, device_id, left, right);
  return root;
}


static void
cupti_device_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "device_id %d: delete", cupti_device_id_map_root->device_id);

  if (cupti_device_id_map_root->left == NULL) {
    cupti_device_id_map_root = cupti_device_id_map_root->right;
  } else {
    cupti_device_id_map_root->left = 
      cupti_device_id_map_splay(cupti_device_id_map_root->left, 
			   cupti_device_id_map_root->device_id);
    cupti_device_id_map_root->left->right = cupti_device_id_map_root->right;
    cupti_device_id_map_root = cupti_device_id_map_root->left;
  }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_device_id_map_entry_t *
cupti_device_id_map_lookup(uint32_t id)
{
  cupti_device_id_map_entry_t *result = NULL;

  cupti_device_id_map_root = cupti_device_id_map_splay(cupti_device_id_map_root, id);
  if (cupti_device_id_map_root && cupti_device_id_map_root->device_id == id) {
    result = cupti_device_id_map_root;
  }

  TMSG(DEFER_CTXT, "device_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cupti_device_id_map_insert(uint32_t device_id)
{
  cupti_device_id_map_entry_t *entry = cupti_device_id_map_entry_new(device_id);
  cuda_device_property_query(device_id, &entry->property); 

  TMSG(DEFER_CTXT, "device_id map insert: id=0x%lx (record %p)", device_id, entry);

  entry->left = entry->right = NULL;

  if (cupti_device_id_map_root != NULL) {
    cupti_device_id_map_root = 
      cupti_device_id_map_splay(cupti_device_id_map_root, device_id);

    if (device_id < cupti_device_id_map_root->device_id) {
      entry->left = cupti_device_id_map_root->left;
      entry->right = cupti_device_id_map_root;
      cupti_device_id_map_root->left = NULL;
    } else if (device_id > cupti_device_id_map_root->device_id) {
      entry->left = cupti_device_id_map_root;
      entry->right = cupti_device_id_map_root->right;
      cupti_device_id_map_root->right = NULL;
    } else {
      // device_id already present: fatal error since a device_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  cupti_device_id_map_root = entry;
}


void
cupti_device_id_map_delete(uint32_t device_id)
{
  cupti_device_id_map_root =
    cupti_device_id_map_splay(cupti_device_id_map_root, device_id);

  if (cupti_device_id_map_root &&
    cupti_device_id_map_root->device_id == device_id) {
    cupti_device_id_map_delete_root();
  }
}


cupti_device_property_t *
cupti_device_id_map_entry_device_property_get
(
 cupti_device_id_map_entry_t *entry
)
{
  return &entry->property;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_device_id_map_count_helper(cupti_device_id_map_entry_t *entry) 
{
  if (entry) {
     int left = cupti_device_id_map_count_helper(entry->left);
     int right = cupti_device_id_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_device_id_map_count() 
{
  return cupti_device_id_map_count_helper(cupti_device_id_map_root);
}

