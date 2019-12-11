//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-correlation-channel-set.h"
#include "gpu-monitoring-thread-api.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_monitoring_thread_activities_ready
(
 void
)
{
  gpu_correlation_channel_set_consume();
}

