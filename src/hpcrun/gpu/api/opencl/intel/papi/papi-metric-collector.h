// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef PAPI_METRIC_COLLECTOR_H_
#define PAPI_METRIC_COLLECTOR_H_

//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../../cct/cct.h"                                             // cct_node_t
#include "../../../../activity/gpu-activity-channel.h"                   // gpu_activity_channel_t



//******************************************************************************
// interface operations
//******************************************************************************

void
intel_papi_setup
(
 void
);


void
intel_papi_teardown
(
 void
);


void
papi_metric_collection_at_kernel_start
(
 uint64_t,
 cct_node_t*,
 gpu_activity_channel_t*
);


void
papi_metric_collection_at_kernel_end
(
 uint64_t
);

#endif      // PAPI_METRIC_COLLECTOR_H_
