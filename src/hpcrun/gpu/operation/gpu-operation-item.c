// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

//******************************************************************************
// system includes
//******************************************************************************




//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "../common/gpu-print.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "../activity/gpu-activity.h"
#include "gpu-operation-item.h"



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_operation_item_dump
(
 const gpu_operation_item_t *item,
 const char *context
)
{
  PRINT("OPERATION_%s: return_channel = %p -> activity = %p | corr = %lu kind = %s, type = %s\n",
         context,
         item->channel,
         &item->activity,
         item->activity.kind == GPU_ACTIVITY_MEMCPY
           ? item->activity.details.memcpy.correlation_id
           : item->activity.details.kernel.correlation_id,
         gpu_kind_to_string(item->activity.kind),
         gpu_type_to_string(item->activity.details.memcpy.copyKind));
}
