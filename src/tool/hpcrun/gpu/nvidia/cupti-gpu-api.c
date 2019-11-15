//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-monitoring-thread-api.h>

#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-process.h>

#include "cupti-activity-translate.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_buffer_completion_notify
(
 void
)
{
  gpu_monitoring_thread_activities_ready();
}


void
cupti_activity_process
(
 CUpti_Activity *cupti_activity
)
{
  gpu_activity_t gpu_activity;
  cupti_activity_translate(&gpu_activity, cupti_activity); 
  gpu_activity_process(&gpu_activity);
}
