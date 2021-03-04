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

#include "cuda-api.h"
#include "sanitizer-context-map.h"
#include "sanitizer-stream-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct sanitizer_context_map_entry_s {
  CUcontext context;
  CUstream priority_stream;
  CUstream kernel_stream;
  CUfunction analysis_function;
  Sanitizer_StreamHandle priority_stream_handle;
  Sanitizer_StreamHandle kernel_stream_handle;
  sanitizer_stream_map_entry_t *streams;
  spinlock_t lock;
  atomic_int lock_thread;
  gpu_patch_buffer_t *buffer_device;
  gpu_patch_buffer_t *buffer_addr_read_device;
  gpu_patch_buffer_t *buffer_addr_write_device;
  gpu_patch_aux_address_dict_t *aux_addr_dict_device;
  gpu_patch_buffer_t *buffer_reset;
  gpu_patch_buffer_t *buffer_addr_read_reset;
  gpu_patch_buffer_t *buffer_addr_write_reset;
  gpu_patch_aux_address_dict_t *aux_addr_dict_reset;
  struct sanitizer_context_map_entry_s *left;
  struct sanitizer_context_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static sanitizer_context_map_entry_t *sanitizer_context_map_root = NULL;
static spinlock_t sanitizer_context_map_lock = SPINLOCK_UNLOCKED;


/******************************************************************************
 * private operations
 *****************************************************************************/

static sanitizer_context_map_entry_t *
sanitizer_context_map_entry_new(CUcontext context)
{
  sanitizer_context_map_entry_t *e;
  e = (sanitizer_context_map_entry_t *)
    hpcrun_malloc(sizeof(sanitizer_context_map_entry_t));
  e->context = context;
  e->priority_stream = NULL;
  e->kernel_stream = NULL;
  e->priority_stream_handle = NULL;
  e->kernel_stream_handle = NULL;
  e->streams = NULL;
  e->left = NULL;
  e->right = NULL;
  e->analysis_function = NULL;
  e->buffer_device = NULL;
  e->buffer_addr_read_device = NULL;
  e->buffer_addr_write_device = NULL;
  e->aux_addr_dict_device = NULL;
  e->buffer_reset = NULL;
  e->buffer_addr_read_reset = NULL;
  e->buffer_addr_write_reset = NULL;
  e->aux_addr_dict_reset = NULL;

  atomic_store(&e->lock_thread, -1);

  spinlock_init(&(e->lock));

  return e;
}


static sanitizer_context_map_entry_t *
sanitizer_context_map_splay(sanitizer_context_map_entry_t *root, CUcontext key)
{
  REGULAR_SPLAY_TREE(sanitizer_context_map_entry_s, root, key, context, left, right);
  return root;
}


static void
sanitizer_context_map_delete_root()
{
  TMSG(DEFER_CTXT, "context %p: delete", sanitizer_context_map_root->context);

  if (sanitizer_context_map_root->left == NULL) {
    sanitizer_context_map_root = sanitizer_context_map_root->right;
  } else {
    sanitizer_context_map_root->left = 
      sanitizer_context_map_splay(sanitizer_context_map_root->left, 
			   sanitizer_context_map_root->context);
    sanitizer_context_map_root->left->right = sanitizer_context_map_root->right;
    sanitizer_context_map_root = sanitizer_context_map_root->left;
  }
}


static sanitizer_context_map_entry_t *
sanitizer_context_map_init_internal(CUcontext context)
{
  sanitizer_context_map_entry_t *entry = NULL;

  if (sanitizer_context_map_root != NULL) {
    sanitizer_context_map_root = 
      sanitizer_context_map_splay(sanitizer_context_map_root, context);

    if (context < sanitizer_context_map_root->context) {
      entry = sanitizer_context_map_entry_new(context);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_context_map_root->left;
      entry->right = sanitizer_context_map_root;
      sanitizer_context_map_root->left = NULL;
      sanitizer_context_map_root = entry;
    } else if (context > sanitizer_context_map_root->context) {
      entry = sanitizer_context_map_entry_new(context);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_context_map_root;
      entry->right = sanitizer_context_map_root->right;
      sanitizer_context_map_root->right = NULL;
      sanitizer_context_map_root = entry;
    } else {
      // context already present
      entry = sanitizer_context_map_root;
    }
  } else {
    entry = sanitizer_context_map_entry_new(context);
    entry->left = entry->right = NULL;
    sanitizer_context_map_root = entry;
  }

  return entry;
}


static sanitizer_context_map_entry_t *
sanitizer_context_map_lookup_internal(CUcontext context)
{
  sanitizer_context_map_entry_t *result = NULL;
  sanitizer_context_map_root = sanitizer_context_map_splay(sanitizer_context_map_root, context);
  if (sanitizer_context_map_root && sanitizer_context_map_root->context == context) {
    result = sanitizer_context_map_root;
  }

  TMSG(DEFER_CTXT, "context map lookup: context=0x%lx (record %p)", context, result);
  return result;
}

/******************************************************************************
 * interface operations
 *****************************************************************************/


sanitizer_context_map_entry_t *
sanitizer_context_map_lookup(CUcontext context)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  spinlock_unlock(&sanitizer_context_map_lock);

  return result;
}


sanitizer_context_map_entry_t *
sanitizer_context_map_init(CUcontext context)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_init_internal(context);

  spinlock_unlock(&sanitizer_context_map_lock);
  
  return result;
}


void
sanitizer_context_map_context_lock(CUcontext context, int32_t thread_id)
{
  sanitizer_context_map_entry_t *entry = NULL;

  spinlock_lock(&sanitizer_context_map_lock);

  entry = sanitizer_context_map_init_internal(context);

  spinlock_unlock(&sanitizer_context_map_lock);

  if (entry != NULL) {
    if (atomic_load(&entry->lock_thread) != thread_id) {
      spinlock_lock(&entry->lock);
      atomic_store(&entry->lock_thread, thread_id);
    }
  }
}


void
sanitizer_context_map_context_unlock(CUcontext context, int32_t thread_id)
{
  sanitizer_context_map_entry_t *entry = NULL;

  spinlock_lock(&sanitizer_context_map_lock);

  entry = sanitizer_context_map_init_internal(context);

  spinlock_unlock(&sanitizer_context_map_lock);

  if (entry != NULL) {
    if (atomic_load(&entry->lock_thread) == thread_id) {
      atomic_store(&entry->lock_thread, -1);
      spinlock_unlock(&entry->lock);
    }
  }
}


void
sanitizer_context_map_delete(CUcontext context)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_root = sanitizer_context_map_splay(sanitizer_context_map_root, context);
  
  if (sanitizer_context_map_root && sanitizer_context_map_root->context == context) {
    sanitizer_context_map_delete_root();
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_insert(CUcontext context, CUstream stream)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_stream_map_insert(&sanitizer_context_map_root->streams, stream);

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_analysis_function_update(CUcontext context, CUfunction function)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *entry = sanitizer_context_map_init_internal(context);

  entry->analysis_function = function;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_priority_stream_handle_update(CUcontext context, Sanitizer_StreamHandle priority_stream_handle)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  if (result->priority_stream_handle == NULL) {
    result->priority_stream_handle = priority_stream_handle;
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_kernel_stream_handle_update(CUcontext context, Sanitizer_StreamHandle kernel_stream_handle)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  if (result->kernel_stream_handle == NULL) {
    result->kernel_stream_handle = kernel_stream_handle;
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_buffer_device_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_device
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->buffer_device = buffer_device;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_buffer_addr_read_device_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_addr_read_device
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->buffer_addr_read_device = buffer_addr_read_device;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_aux_addr_dict_device_update
(
 CUcontext context,
 gpu_patch_aux_address_dict_t *aux_addr_dict_device
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->aux_addr_dict_device = aux_addr_dict_device;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_buffer_addr_write_device_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_addr_write_device
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->buffer_addr_write_device = buffer_addr_write_device;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_buffer_reset_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_reset
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->buffer_reset = buffer_reset;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_buffer_addr_read_reset_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_addr_read_reset
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->buffer_addr_read_reset = buffer_addr_read_reset;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_aux_addr_dict_reset_update
(
 CUcontext context,
 gpu_patch_aux_address_dict_t *aux_addr_dict_reset
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->aux_addr_dict_reset = aux_addr_dict_reset;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_buffer_addr_write_reset_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_addr_write_reset
)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  result->buffer_addr_write_reset = buffer_addr_write_reset;

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_stream_lock
(
 CUcontext context,
 CUstream stream
)
{
  sanitizer_context_map_entry_t *entry = NULL;

  spinlock_lock(&sanitizer_context_map_lock);

  if ((entry = sanitizer_context_map_lookup_internal(context)) != NULL) {
    sanitizer_stream_map_stream_lock(&entry->streams, stream);
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_stream_unlock
(
 CUcontext context,
 CUstream stream
)
{
  sanitizer_context_map_entry_t *entry = NULL;

  spinlock_lock(&sanitizer_context_map_lock);

  if ((entry = sanitizer_context_map_lookup_internal(context)) != NULL) {
    sanitizer_stream_map_stream_unlock(&entry->streams, stream);
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


CUstream
sanitizer_context_map_entry_priority_stream_get
(
 sanitizer_context_map_entry_t *entry
)
{
  if (entry->priority_stream == NULL) {
    spinlock_lock(&sanitizer_context_map_lock);
    entry->priority_stream = cuda_priority_stream_create();
    spinlock_unlock(&sanitizer_context_map_lock);
  }
  return entry->priority_stream;
}


CUstream
sanitizer_context_map_entry_kernel_stream_get
(
 sanitizer_context_map_entry_t *entry
)
{
  if (entry->kernel_stream == NULL) {
    spinlock_lock(&sanitizer_context_map_lock);
    entry->kernel_stream = cuda_stream_create();
    spinlock_unlock(&sanitizer_context_map_lock);
  }
  return entry->kernel_stream;
}


Sanitizer_StreamHandle
sanitizer_context_map_entry_priority_stream_handle_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->priority_stream_handle;
}


Sanitizer_StreamHandle
sanitizer_context_map_entry_kernel_stream_handle_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->kernel_stream_handle;
}


CUfunction
sanitizer_context_map_entry_analysis_function_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->analysis_function;
}


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_device_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->buffer_device;
}


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_addr_read_device_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->buffer_addr_read_device;
}


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_addr_write_device_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->buffer_addr_write_device;
}


gpu_patch_aux_address_dict_t *
sanitizer_context_map_entry_aux_addr_dict_device_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->aux_addr_dict_device;
}


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_reset_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->buffer_reset;
}


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_addr_read_reset_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->buffer_addr_read_reset;
}


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_addr_write_reset_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->buffer_addr_write_reset;
}


gpu_patch_aux_address_dict_t *
sanitizer_context_map_entry_aux_addr_dict_reset_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->aux_addr_dict_reset;
}
