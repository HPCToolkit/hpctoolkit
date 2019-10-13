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

#include "cupti-host-op-map.h"
#include "cupti-channel.h"

#define CUPTI_HOST_OP_MAP_DEBUG 0

#if CUPTI_HOST_OP_MAP_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

/******************************************************************************
 * type definitions 
 *****************************************************************************/
struct cupti_host_op_map_entry_s {
  uint64_t host_op_id;
  int samples;
  int total_samples;
  cupti_activity_channel_t *channel;
  cct_node_t *copy_node;
  cct_node_t *copyin_node;
  cct_node_t *copyout_node;
  cct_node_t *alloc_node;
  cct_node_t *delete_node;
  cct_node_t *sync_node;
  cct_node_t *kernel_node;
  cct_node_t *trace_node;
  struct cupti_host_op_map_entry_s *left;
  struct cupti_host_op_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_host_op_map_entry_t *cupti_host_op_map_root = NULL;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_host_op_map_entry_t *
cupti_host_op_map_entry_new(uint64_t host_op_id, cct_node_t *copy_node,
  cct_node_t *copyin_node, cct_node_t *copyout_node, cct_node_t *alloc_node,
  cct_node_t *delete_node, cct_node_t *sync_node, cct_node_t *kernel_node,
  cct_node_t *trace_node, cupti_activity_channel_t *channel)
{
  cupti_host_op_map_entry_t *e;
  e = (cupti_host_op_map_entry_t *)hpcrun_malloc_safe(sizeof(cupti_host_op_map_entry_t));
  e->host_op_id = host_op_id;
  e->samples = 0;
  e->total_samples = 0;
  e->channel = channel;
  e->left = NULL;
  e->right = NULL;
  e->copy_node = copy_node;
  e->copyin_node = copyin_node;
  e->copyout_node = copyout_node;
  e->alloc_node = alloc_node;
  e->delete_node = delete_node;
  e->sync_node = sync_node;
  e->kernel_node = kernel_node;
  e->trace_node = trace_node;

  return e;
}


static cupti_host_op_map_entry_t *
cupti_host_op_map_splay(cupti_host_op_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(cupti_host_op_map_entry_s, root, key, host_op_id, left, right);
  return root;
}


static void
cupti_host_op_map_delete_root()
{
  TMSG(DEFER_CTXT, "host op %d: delete", cupti_host_op_map_root->host_op_id);

  if (cupti_host_op_map_root->left == NULL) {
    cupti_host_op_map_root = cupti_host_op_map_root->right;
  } else {
    cupti_host_op_map_root->left = 
      cupti_host_op_map_splay(cupti_host_op_map_root->left, 
        cupti_host_op_map_root->host_op_id);
    cupti_host_op_map_root->left->right = cupti_host_op_map_root->right;
    cupti_host_op_map_root = cupti_host_op_map_root->left;
  }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_host_op_map_entry_t *
cupti_host_op_map_lookup(uint64_t id)
{
  cupti_host_op_map_entry_t *result = NULL;

  cupti_host_op_map_root = cupti_host_op_map_splay(cupti_host_op_map_root, id);
  if (cupti_host_op_map_root && cupti_host_op_map_root->host_op_id == id) {
    result = cupti_host_op_map_root;
  }

  TMSG(DEFER_CTXT, "host op map lookup: id=0x%lx (channel %p)", id, result);
  return result;
}


void
cupti_host_op_map_insert(uint64_t host_op_id, cupti_activity_channel_t *channel, 
  cct_node_t *copy_node, cct_node_t *copyin_node, cct_node_t *copyout_node,
  cct_node_t *alloc_node, cct_node_t *delete_node, cct_node_t *sync_node,
  cct_node_t *kernel_node, cct_node_t *trace_node)
{
  cupti_host_op_map_entry_t *entry = cupti_host_op_map_entry_new(
    host_op_id, copy_node, copyin_node, copyout_node, alloc_node, delete_node,
    sync_node, kernel_node, trace_node, channel);

  TMSG(DEFER_CTXT, "host op map insert: id=0x%lx", host_op_id);

  entry->left = entry->right = NULL;

  if (cupti_host_op_map_root != NULL) {
    cupti_host_op_map_root = 
      cupti_host_op_map_splay(cupti_host_op_map_root, host_op_id);

    if (host_op_id < cupti_host_op_map_root->host_op_id) {
      entry->left = cupti_host_op_map_root->left;
      entry->right = cupti_host_op_map_root;
      cupti_host_op_map_root->left = NULL;
    } else if (host_op_id > cupti_host_op_map_root->host_op_id) {
      entry->left = cupti_host_op_map_root;
      entry->right = cupti_host_op_map_root->right;
      cupti_host_op_map_root->right = NULL;
    } else {
      // host_op_id already present: fatal error since a host_op_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  
  cupti_host_op_map_root = entry;
}


bool
cupti_host_op_map_samples_increase(uint64_t host_op_id, int val)
{
  bool result = true; 

  TMSG(DEFER_CTXT, "host op map samples update: id=0x%lx (update %d)", 
       host_op_id, val);

  cupti_host_op_map_root = cupti_host_op_map_splay(cupti_host_op_map_root, host_op_id);

  if (cupti_host_op_map_root && cupti_host_op_map_root->host_op_id == host_op_id) {
    cupti_host_op_map_root->samples += val;
    PRINT("total %d curr %d\n", cupti_host_op_map_root->total_samples, cupti_host_op_map_root->samples);
    if (cupti_host_op_map_root->samples == cupti_host_op_map_root->total_samples) {
      PRINT("sample root deleted\n");
      cupti_host_op_map_delete_root();
      result = false;
    }
  }

  return result;
}


bool
cupti_host_op_map_total_samples_update(uint64_t host_op_id, int val)
{
  bool result = true; 

  TMSG(DEFER_CTXT, "host op map total samples update: id=0x%lx (update %d)", 
       host_op_id, val);

  cupti_host_op_map_root = cupti_host_op_map_splay(cupti_host_op_map_root, host_op_id);

  PRINT("cupti_host_op_map_total_samples_update\n");
  if (cupti_host_op_map_root && cupti_host_op_map_root->host_op_id == host_op_id) {
    cupti_host_op_map_root->total_samples = val;
    PRINT("total %d curr %d\n", cupti_host_op_map_root->total_samples, cupti_host_op_map_root->samples);
    if (cupti_host_op_map_root->samples == cupti_host_op_map_root->total_samples) {
      result = false;
    }
  }

  return result;
}


void
cupti_host_op_map_delete(uint64_t host_op_id)
{
  cupti_host_op_map_root = cupti_host_op_map_splay(cupti_host_op_map_root, host_op_id);

  if (cupti_host_op_map_root && cupti_host_op_map_root->host_op_id == host_op_id) {
    cupti_host_op_map_delete_root();
  }
}


cct_node_t *
cupti_host_op_map_entry_copy_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->copy_node;
}


cct_node_t *
cupti_host_op_map_entry_copyin_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->copyin_node;
}


cct_node_t *
cupti_host_op_map_entry_copyout_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->copyout_node;
}


cct_node_t *
cupti_host_op_map_entry_alloc_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->alloc_node;
}


cct_node_t *
cupti_host_op_map_entry_delete_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->delete_node;
}


cct_node_t *
cupti_host_op_map_entry_sync_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->sync_node;
}


cct_node_t *
cupti_host_op_map_entry_kernel_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->kernel_node;
}


cct_node_t *
cupti_host_op_map_entry_trace_node_get
(
 cupti_host_op_map_entry_t *entry
)
{
  return entry->trace_node;
}


cupti_activity_channel_t *
cupti_host_op_map_entry_activity_channel_get(cupti_host_op_map_entry_t *entry)
{
  return entry->channel;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_host_op_map_count_helper(cupti_host_op_map_entry_t *entry) 
{
  if (entry) {
    int left = cupti_host_op_map_count_helper(entry->left);
    int right = cupti_host_op_map_count_helper(entry->right);
    return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_host_op_map_count() 
{
  return cupti_host_op_map_count_helper(cupti_host_op_map_root);
}
