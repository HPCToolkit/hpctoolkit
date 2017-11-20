#ifndef _CUPTI_ACTIVITY_STRINGS_H_
#define _CUPTI_ACTIVITY_STRINGS_H_

#include <cupti_activity.h>

//******************************************************************************
// macros
//******************************************************************************

#define FOREACH_ACTIVITY_KIND(macro)	\
  macro(INVALID)			\
  macro(MEMCPY)				\
  macro(MEMSET)				\
  macro(KERNEL)				\
  macro(DRIVER)				\
  macro(RUNTIME)			\
  macro(EVENT)				\
  macro(METRIC)				\
  macro(DEVICE)				\
  macro(CONTEXT)			\
  macro(CONCURRENT_KERNEL)		\
  macro(NAME)				\
  macro(MARKER)				\
  macro(MARKER_DATA)			\
  macro(SOURCE_LOCATOR)			\
  macro(GLOBAL_ACCESS)			\
  macro(BRANCH)				\
  macro(OVERHEAD)			\
  macro(CDP_KERNEL)			\
  macro(PREEMPTION)			\
  macro(ENVIRONMENT)			\
  macro(EVENT_INSTANCE)			\
  macro(MEMCPY2)			\
  macro(METRIC_INSTANCE)		\
  macro(INSTRUCTION_EXECUTION)		\
  macro(UNIFIED_MEMORY_COUNTER)		\
  macro(FUNCTION)			\
  macro(MODULE)				\
  macro(DEVICE_ATTRIBUTE)		\
  macro(SHARED_ACCESS)			\
  macro(PC_SAMPLING)			\
  macro(PC_SAMPLING_RECORD_INFO)	\
  macro(INSTRUCTION_CORRELATION)	\
  macro(OPENACC_DATA)			\
  macro(OPENACC_LAUNCH)			\
  macro(OPENACC_OTHER)			\
  macro(CUDA_EVENT)			\
  macro(STREAM)				\
  macro(SYNCHRONIZATION)		\
  macro(EXTERNAL_CORRELATION)		\
  macro(NVLINK)				\
  macro(INSTANTANEOUS_EVENT)		\
  macro(INSTANTANEOUS_EVENT_INSTANCE)	\
  macro(INSTANTANEOUS_METRIC)		\
  macro(INSTANTANEOUS_METRIC_INSTANCE)	\
  macro(FORCE_INT)



//******************************************************************************
// types
//******************************************************************************

typedef void (*cupti_activity_fn_t) 
(
 CUpti_Activity *activity,
 void *state
);


typedef struct {
#define macro(kind) cupti_activity_fn_t kind;
  FOREACH_ACTIVITY_KIND(macro)
#undef macro
} cupti_activity_dispatch_t;

//******************************************************************************
// interface operations
//******************************************************************************

extern const char*
cupti_status_to_string
(
 uint32_t err
);


extern const char *
cupti_activity_overhead_kind_string
(
 CUpti_ActivityOverheadKind kind
);


extern const char *
cupti_memcpy_kind_string
(
 CUpti_ActivityMemcpyKind kind
);


extern const char *
cupti_stall_reason_string
(
 CUpti_ActivityPCSamplingStallReason kind
) ;


extern const char *
cupti_activity_object_kind_string
(
 CUpti_ActivityObjectKind kind
);


extern uint32_t 
cupti_activity_object_kind_id
(
 CUpti_ActivityObjectKind kind, 
 CUpti_ActivityObjectKindId *kid 
);



#endif // __CUPTI_ACTIVITY_STRINGS_H__
