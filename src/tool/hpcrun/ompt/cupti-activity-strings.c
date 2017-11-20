#include "cupti-activity-strings.h"

#define FOREACH_CUPTI_STATUS(macro)	              \
  macro(CUPTI_SUCCESS)				      \
  macro(CUPTI_ERROR_INVALID_PARAMETER)		      \
  macro(CUPTI_ERROR_INVALID_DEVICE)		      \
  macro(CUPTI_ERROR_INVALID_CONTEXT)		      \
  macro(CUPTI_ERROR_NOT_INITIALIZED) 	


#define FOREACH_ACTIVITY_OVERHEAD(macro)	      \
  macro(DRIVER, COMPILER)			      \
  macro(CUPTI,  BUFFER_FLUSH)			      \
  macro(CUPTI,  INSTRUMENTATION)		      \
  macro(CUPTI,  RESOURCE)


#define FOREACH_OBJECT_KIND(macro)	              \
  macro(PROCESS, pt.processId)			      \
  macro(THREAD,  pt.threadId)			      \
  macro(DEVICE,  dcs.deviceId)			      \
  macro(CONTEXT, dcs.contextId)			      \
  macro(STREAM,  dcs.streamId)


#define FOREACH_MEMCPY_KIND(macro)                    \
  macro(ATOA)					      \
  macro(ATOD)					      \
  macro(ATOH)					      \
  macro(DTOA)					      \
  macro(DTOD)					      \
  macro(DTOH)					      \
  macro(HTOA)					      \
  macro(HTOD)					      \
  macro(HTOH)


#define FOREACH_STALL_REASON(macro)                   \
  macro(INVALID)				      \
  macro(NONE)					      \
  macro(INST_FETCH)				      \
  macro(EXEC_DEPENDENCY)			      \
  macro(MEMORY_DEPENDENCY)			      \
  macro(TEXTURE)				      \
  macro(SYNC)					      \
  macro(CONSTANT_MEMORY_DEPENDENCY)		      \
  macro(PIPE_BUSY)				      \
  macro(MEMORY_THROTTLE)			      \
  macro(NOT_SELECTED)				      \
  macro(OTHER)



#define gpu_event_decl(stall) \
  static int gpu_stall_event_ ## stall; 

FOREACH_STALL_REASON(gpu_event_decl)


//-------------------------------------------------------------
// printing support
//-------------------------------------------------------------

const char*
cupti_status_to_string
(
 uint32_t err
)
{
#define CUPTI_STATUS_TO_STRING(s) if (err == s) return #s;
  
  FOREACH_CUPTI_STATUS(CUPTI_STATUS_TO_STRING);

#undef CUPTI_STATUS_TO_STRING			
  
  return "CUPTI_STATUS_UNKNOWN";
}



const char *
cupti_activity_overhead_kind_string
(
 CUpti_ActivityOverheadKind kind
)
{
#define macro(oclass, otype)						\
  case CUPTI_ACTIVITY_OVERHEAD_ ## oclass ## _ ## otype: return #otype;

  switch(kind) {
    FOREACH_ACTIVITY_OVERHEAD(macro)
  default: return "UNKNOWN";
  }

#undef macro
}


const char *
cupti_memcpy_kind_string
(
 CUpti_ActivityMemcpyKind kind
)
{
#define macro(name)						\
  case CUPTI_ACTIVITY_MEMCPY_KIND_ ## name: return #name;

  switch(kind) {
    FOREACH_MEMCPY_KIND(macro)
  default: return "UNKNOWN";
  }

#undef macro
}


const char *
cupti_stall_reason_string
(
 CUpti_ActivityPCSamplingStallReason kind
)
{
#define macro(stall)							\
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_ ## stall: return #stall; 
  
  switch (kind) {
    FOREACH_STALL_REASON(macro)
  default: return "UNKNOWN";
  }

#undef macro 
}


const char *
cupti_activity_object_kind_string
(
 CUpti_ActivityObjectKind kind
)
{
#define macro(name, id)					\
  case CUPTI_ACTIVITY_OBJECT_ ## name: return #name;

  switch(kind) {
    FOREACH_OBJECT_KIND(macro)
  default: return "UNKNOWN";
  }

#undef macro
}


uint32_t 
cupti_activity_object_kind_id
(
 CUpti_ActivityObjectKind kind, 
 CUpti_ActivityObjectKindId *kid 
)
{
#define macro(name, id)					\
  case CUPTI_ACTIVITY_OBJECT_ ## name: return kid->id;

  switch(kind) {
    FOREACH_OBJECT_KIND(macro)
  default: return -1;
  }

#undef macro
}

