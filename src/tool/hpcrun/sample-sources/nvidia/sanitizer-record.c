#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include "sanitizer-record.h"
#include "sanitizer-context-map.h"
#include "sanitizer-stream-map.h"
#include "cstack.h"


// shared by multiple threads
static cstack_t free_notification_stack = { .head = {NULL} };
static cstack_t memory_buffer_stack = { .head = {NULL} };


//------------------------------------------------------
// notification handlers
//------------------------------------------------------

void
sanitizer_record_init()
{
  cstack_init(&free_notification_stack);
  cstack_init(&memory_buffer_stack);
}


void
sanitizer_notification_insert
(
 CUcontext context,
 CUmodule module,
 CUstream stream,
 uint64_t function_addr, 
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device,
 cct_node_t *host_op_node
)
{
  cstack_node_t *node = cstack_pop(&free_notification_stack);
  if (node == NULL) {
    node = sanitizer_notification_node_new(module, stream, function_addr,
      host_op_node, type, buffer_device);
  } else {
    sanitizer_notification_node_set(node, module, stream, function_addr,
      host_op_node, type, buffer_device);
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
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device
)
{
  switch (type) {
    case SANITIZER_ACTIVITY_TYPE_MEMORY:
      {
        cstack_push(&memory_buffer_stack, buffer_device);
        break;
      }
    default:
      {
        break;
      }
  }
}


cstack_node_t *
sanitizer_buffer_device_pop
(
 sanitizer_activity_type_t type
)
{
  cstack_node_t *buffer_device = NULL;
  switch (type) {
    case SANITIZER_ACTIVITY_TYPE_MEMORY:
      {
        buffer_device = cstack_pop(&memory_buffer_stack);
        break;
      }
    default:
      {
        break;
      }
  }
  return buffer_device;
}
