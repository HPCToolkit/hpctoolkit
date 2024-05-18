// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_application_thread_api_h
#define gpu_application_thread_api_h



//******************************************************************************
// local includes
//******************************************************************************

#include "activity/gpu-activity.h"



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_application_thread_process_activities
(
 void
);


cct_node_t *
gpu_application_thread_correlation_callback
(
 uint64_t correlation_id
);


cct_node_t *
gpu_application_thread_correlation_callback_rt1
(
 uint64_t correlation_id
);


#endif
