#ifndef _HPCTOOLKIT_CUPTI_API_H_
#define _HPCTOOLKIT_CUPTI_API_H_

#include <hpcrun/loadmap.h>
#include <cupti.h>
#include "cupti-node.h"

//******************************************************************************
// constants
//******************************************************************************

extern CUpti_ActivityKind
external_correlation_activities[];

extern CUpti_ActivityKind
data_motion_explicit_activities[];

extern CUpti_ActivityKind
data_motion_implicit_activities[];

extern CUpti_ActivityKind
kernel_invocation_activities[];

extern CUpti_ActivityKind
kernel_execution_activities[];

extern CUpti_ActivityKind
driver_activities[];

extern CUpti_ActivityKind
runtime_activities[];

extern CUpti_ActivityKind
overhead_activities[];

typedef enum {
  cupti_set_all = 1,
  cupti_set_some = 2,
  cupti_set_none = 3
} cupti_set_status_t;


//******************************************************************************
// interface functions
//******************************************************************************

int
cupti_bind
(
 void
);


void
cupti_activity_process
(
 CUpti_Activity *activity
);


void 
cupti_buffer_alloc 
(
 uint8_t **buffer, 
 size_t *buffer_size, 
 size_t *maxNumRecords
);


void
cupti_callbacks_subscribe
(
);


void
cupti_callbacks_unsubscribe
(
);


void
cupti_correlation_enable
(
);


void
cupti_correlation_disable
(
);


void
cupti_pc_sampling_enable
(
 CUcontext context,
 int frequency 
);


void
cupti_pc_sampling_disable
(
 CUcontext context
);


cupti_set_status_t 
cupti_monitoring_set
(
 CUcontext context,
 const  CUpti_ActivityKind activity_kinds[],
 bool enable
);


void
cupti_device_timestamp_get
(
 CUcontext context,
 uint64_t *time
);


void 
cupti_trace_init
(
);


void 
cupti_trace_start
(
);


void 
cupti_trace_pause
(
 CUcontext context,
 bool begin_pause
);


void 
cupti_trace_finalize
(
);


void
cupti_device_buffer_config
(
 size_t buf_size,
 size_t sem_size
);


void
cupti_num_dropped_records_get
(
 CUcontext context,
 uint32_t streamId,
 size_t* dropped 
);


bool
cupti_buffer_cursor_advance
(
  uint8_t *buffer,
  size_t size,
  CUpti_Activity **current
);


void 
cupti_buffer_completion_callback
(
 CUcontext ctx,
 uint32_t streamId,
 uint8_t *buffer,
 size_t size,
 size_t validSize
);


void
cupti_load_callback_cuda
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
);


void
cupti_unload_callback_cuda
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
);


//******************************************************************************
// finalizer
//******************************************************************************

void
cupti_activity_flush
(
);


void
cupti_device_flush
(
 void *args
);


void
cupti_device_shutdown
(
 void *args
);


void
cupti_stop_flag_set
(
);


void
cupti_stop_flag_unset
(
);

//******************************************************************************
// ignores
//******************************************************************************

bool
cupti_lm_contains_fn
(
 const char *lm,
 const char *fn
);


bool
cupti_modules_ignore
(
 load_module_t *module
);

//******************************************************************************
// notification stack
//******************************************************************************

void
cupti_notification_handle
(
 cupti_node_t *node
);

void
cupti_activity_handle
(
 cupti_node_t *node
);

#endif
