// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_monitoring_thread_api_h
#define gpu_monitoring_thread_api_h

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_monitoring_thread_activities_ready
(
 void
);


void
gpu_monitoring_thread_activities_ready_with_idx
(
 uint64_t idx
);


#endif
