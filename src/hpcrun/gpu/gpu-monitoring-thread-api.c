// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// global includes
//******************************************************************************

#include <stdlib.h>
#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "activity/correlation/gpu-correlation-channel.h"
#include "activity/correlation/gpu-host-correlation-map.h"
#include "gpu-monitoring-thread-api.h"



//******************************************************************************
// private operations
//******************************************************************************

void
receive_correlation
(
  uint64_t correlation_id,
  gpu_activity_channel_t *activity_channel,
  void *arg
)
{
  gpu_host_correlation_map_insert(correlation_id, activity_channel);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_monitoring_thread_activities_ready
(
 void
)
{
  gpu_monitoring_thread_activities_ready_with_idx(0);
}

void
gpu_monitoring_thread_activities_ready_with_idx
(
 uint64_t idx
)
{
  gpu_correlation_channel_receive(idx, receive_correlation, NULL);
}
