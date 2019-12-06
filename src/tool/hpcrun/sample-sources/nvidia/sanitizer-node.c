#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/cct/cct.h>

#include "sanitizer-node.h"


cstack_node_t *
sanitizer_notification_node_new
(
 CUmodule module,
 CUcontext context,
 CUstream stream,
 uint64_t function_addr, 
 cstack_node_t *buffer_device,
 cct_node_t *host_op_node,
 dim3 grid_size,
 dim3 block_size
)
{
  cstack_node_t *node = (cstack_node_t *)hpcrun_malloc_safe(sizeof(cstack_node_t));
  sanitizer_entry_notification_t *entry = (sanitizer_entry_notification_t *)
    hpcrun_malloc_safe(sizeof(sanitizer_entry_notification_t));
  node->entry = entry;
  sanitizer_notification_node_set(node, module, context, stream,
    function_addr, buffer_device, host_op_node, grid_size, block_size);
  return node;
}


void
sanitizer_notification_node_set
(
 cstack_node_t *node,
 CUmodule module,
 CUcontext context,
 CUstream stream,
 uint64_t function_addr, 
 cstack_node_t *buffer_device,
 cct_node_t *host_op_node,
 dim3 grid_size,
 dim3 block_size
)
{
  sanitizer_entry_notification_t *entry = (sanitizer_entry_notification_t *)node->entry;
  entry->module = module;
  entry->context = context;
  entry->stream = stream;
  entry->function_addr = function_addr;
  entry->buffer_device = buffer_device;
  entry->host_op_node = host_op_node;
  entry->grid_size = grid_size;
  entry->block_size = block_size;
}


cstack_node_t *
sanitizer_buffer_node_new
(
 void *buffer
)
{
  cstack_node_t *node = (cstack_node_t *)hpcrun_malloc_safe(sizeof(cstack_node_t));
  sanitizer_entry_buffer_t *entry = (sanitizer_entry_buffer_t *)
    hpcrun_malloc_safe(sizeof(sanitizer_entry_buffer_t));
  node->entry = entry;
  sanitizer_buffer_node_set(node, buffer);
  return node;
}


void
sanitizer_buffer_node_set
(
 cstack_node_t *node,
 void *buffer
)
{
  sanitizer_entry_buffer_t *entry = (sanitizer_entry_buffer_t *)node->entry;
  entry->buffer = buffer;
}
