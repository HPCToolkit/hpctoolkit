#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/cct/cct.h>

#include "sanitizer-node.h"


sanitizer_entry_notification_t *
sanitizer_notification_entry_new
(
 CUmodule module,
 CUcontext context,
 CUstream stream,
 uint64_t function_addr, 
 void *buffer_device,
 cct_node_t *host_op_node,
 dim3 grid_size,
 dim3 block_size
)
{
  sanitizer_entry_notification_t *entry = (sanitizer_entry_notification_t *)
    hpcrun_malloc_safe(sizeof(sanitizer_entry_notification_t));
  sanitizer_notification_entry_set(entry, module, context, stream,
    function_addr, buffer_device, host_op_node, grid_size, block_size);
  return entry;
}


void
sanitizer_notification_entry_set
(
 sanitizer_entry_notification_t *entry,
 CUmodule module,
 CUcontext context,
 CUstream stream,
 uint64_t function_addr, 
 void *buffer_device,
 cct_node_t *host_op_node,
 dim3 grid_size,
 dim3 block_size
)
{
  entry->module = module;
  entry->context = context;
  entry->stream = stream;
  entry->function_addr = function_addr;
  entry->buffer_device = buffer_device;
  entry->host_op_node = host_op_node;
  entry->grid_size = grid_size;
  entry->block_size = block_size;
}
