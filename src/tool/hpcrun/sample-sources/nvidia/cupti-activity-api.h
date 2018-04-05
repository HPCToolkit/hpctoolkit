#ifndef _HPCTOOLKIT_CUPTI_ACTIVITY_API_H_
#define _HPCTOOLKIT_CUPTI_ACTIVITY_API_H_

#include <hpcrun/loadmap.h>
#include <cupti.h>

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

extern void
cupti_activity_process
(
 CUpti_Activity *activity
);


extern void 
cupti_buffer_alloc 
(
 uint8_t **buffer, 
 size_t *buffer_size, 
 size_t *maxNumRecords
);


extern void
cupti_callbacks_subscribe
(
);


extern void
cupti_callbacks_unsubscribe
(
);


extern void
cupti_correlation_enable
(
);


extern void
cupti_correlation_disable
(
 CUcontext context
);


extern cupti_set_status_t 
cupti_monitoring_set
(
 const  CUpti_ActivityKind activity_kinds[],
 bool enable
);


extern void
cupti_device_timestamp_get
(
 CUcontext context,
 uint64_t *time
);


extern void 
cupti_trace_init
(
);


extern void 
cupti_trace_start
(
);


extern void 
cupti_trace_pause
(
 CUcontext context,
 bool begin_pause
);


extern void 
cupti_trace_finalize
(
);


extern void
cupti_num_dropped_records_get
(
 CUcontext context,
 uint32_t streamId,
 size_t* dropped 
);


extern void 
cupti_buffer_completion_callback
(
 CUcontext ctx,
 uint32_t streamId,
 uint8_t *buffer,
 size_t size,
 size_t validSize
);


extern void
cupti_pc_sampling_config
(
  CUcontext context,
  CUpti_ActivityPCSamplingPeriod period
);


extern void
cupti_metrics_init
(
);

//******************************************************************************
// finalizer
//******************************************************************************

extern void
cupti_activity_flush
(
);


extern void
cupti_device_flush
(
 void *args
);


extern void
cupti_device_shutdown
(
 void *args
);

//******************************************************************************
// ignores
//******************************************************************************

extern bool
cupti_lm_contains_fn
(
 const char *lm,
 const char *fn
);


extern bool
cupti_modules_ignore
(
 load_module_t *module
);

#endif
