#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include "sanitizer-record.h"
#include "sanitizer-context-map.h"
#include "sanitizer-stream-map.h"
#include "cstack.h"


// shared by multiple threads
static cstack_t free_notification_stack = { .head = {NULL} };
static cstack_t device_buffer_stack = { .head = {NULL} };


//------------------------------------------------------
// notification handlers
//------------------------------------------------------

void
sanitizer_record_init()
{
  cstack_init(&free_notification_stack);
  cstack_init(&device_buffer_stack);
}


void
sanitizer_notification_insert
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
  cstack_node_t *node = cstack_pop(&free_notification_stack);
  if (node == NULL) {
    node = sanitizer_notification_node_new(module, context, stream, function_addr,
      buffer_device, host_op_node, grid_size, block_size);
  } else {
    sanitizer_notification_node_set(node, module, context, stream, function_addr,
      buffer_device, host_op_node, grid_size, block_size);
  }
  sanitizer_context_map_insert(context, stream, node);
}


void
sanitizer_notification_apply
(
 cstack_t *notification_stack,
 cstack_fn_t fn
)
{
  cstack_apply(notification_stack, &free_notification_stack, fn);
}

//------------------------------------------------------
// buffer handlers
//------------------------------------------------------

void
sanitizer_buffer_device_push
(
 cstack_node_t *buffer_device
)
{
  cstack_push(&device_buffer_stack, buffer_device);
}


cstack_node_t *
sanitizer_buffer_device_pop
(
)
{
  return cstack_pop(&device_buffer_stack);
}
