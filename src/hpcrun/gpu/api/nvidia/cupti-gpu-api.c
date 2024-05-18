// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../gpu-monitoring-thread-api.h"

#include "../../activity/gpu-activity.h"
#include "../../activity/gpu-activity-channel.h"

#include "../../../messages/messages.h"

#include "cupti-activity-translate.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_activity_process
(
 CUpti_Activity *cupti_activity
)
{
  gpu_activity_t gpu_activity;
  uint64_t correlation_id;
  if (!cupti_activity_translate(&gpu_activity, cupti_activity, &correlation_id)) {
    return;
  }

  uint32_t thread_id =
    gpu_activity_channel_correlation_id_get_thread_id(correlation_id);
  gpu_activity_channel_t *channel = gpu_activity_channel_lookup(thread_id);
  if (channel == NULL) {
    TMSG(CUPTI_ACTIVITY, "Cannot find gpu_activity_channel "
                         "(correlation ID: %" PRIu64 ")", correlation_id);
    return;
  }

  gpu_activity_channel_send(channel, &gpu_activity);
}
