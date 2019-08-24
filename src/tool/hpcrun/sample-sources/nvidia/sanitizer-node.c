#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/cct/cct.h>

#include "sanitizer-node.h"


cstack_node_t *
sanitizer_notification_node_new
(
 CUmodule module,
 CUstream stream,
 uint64_t function_addr,
 cct_node_t *host_op_node,
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device
)
{
  cstack_node_t *node = (cstack_node_t *)hpcrun_malloc_safe(sizeof(cstack_node_t));
  sanitizer_entry_notification_t *entry = (sanitizer_entry_notification_t *)
    hpcrun_malloc_safe(sizeof(sanitizer_entry_notification_t));
  node->entry = entry;
  node->type = CSTACK_TYPE_SANITIZER_NOTIFICATION;
  sanitizer_notification_node_set(node, module, stream, function_addr, host_op_node,
    type, buffer_device);
  return node;
}


void
sanitizer_notification_node_set
(
 cstack_node_t *node,
 CUmodule module,
 CUstream stream,
 uint64_t function_addr,
 cct_node_t *host_op_node,
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device
)
{
  sanitizer_entry_notification_t *entry = (sanitizer_entry_notification_t *)node->entry;
  entry->module = module;
  entry->stream = stream;
  entry->function_addr = function_addr;
  entry->host_op_node = host_op_node;
  entry->type = type;
  entry->buffer_device = buffer_device;
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
  node->type = CSTACK_TYPE_SANITIZER_BUFFER;
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
