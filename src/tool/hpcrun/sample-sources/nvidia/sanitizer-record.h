#ifndef _HPCTOOLKIT_SANITIZER_RECORD_H_
#define _HPCTOOLKIT_SANITIZER_RECORD_H_

#include <cuda.h>
#include "sanitizer-node.h"
#include "cstack.h"


typedef void (*sanitizer_record_fn_t)(cstack_t *stack);


void
sanitizer_record_init();


void
sanitizer_notification_insert
(
 CUcontext context,
 CUmodule module,
 CUstream stream,
 uint32_t function_index, 
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device,
 cct_node_t *host_op_node
);


void
sanitizer_notification_apply
(
 cstack_t *notification_stack,
 cstack_fn_t fn
);


void
sanitizer_buffer_device_push
(
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device
);


cstack_node_t *
sanitizer_buffer_device_pop
(
 sanitizer_activity_type_t type
);

#endif
