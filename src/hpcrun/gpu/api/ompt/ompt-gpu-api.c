// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../messages/messages.h"

#include "../../gpu-monitoring-thread-api.h"

#include "../../activity/gpu-activity.h"
#include "../../activity/gpu-activity-channel.h"
#include "../../activity/gpu-activity-process.h"
#include "../../activity/correlation/gpu-host-correlation-map.h"

#include "ompt-gpu-api.h"
#include "ompt-activity-translate.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
ompt_buffer_completion_notify
(
 void
)
{
  gpu_monitoring_thread_activities_ready();
}


void
ompt_activity_process
(
 ompt_record_ompt_t *record
)
{
  gpu_activity_t gpu_activity;
  uint64_t correlation_id;
  ompt_activity_translate(&gpu_activity, record, &correlation_id);

  if (correlation_id == 0) {
    return;
  }

  uint32_t thread_id =
    gpu_activity_channel_correlation_id_get_thread_id(correlation_id);
  gpu_activity_channel_t *channel = gpu_activity_channel_lookup(thread_id);
  if (channel == NULL) {
    TMSG(GPU, "Cannot find gpu_activity_channel "
              "(correlation ID: %" PRIu64 ")", correlation_id);
    return;
  }

  gpu_activity_channel_send(channel, &gpu_activity);
}
