// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_activity_process_h
#define gpu_activity_process_h 1



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_activity_process
(
 gpu_activity_t *ga
);



#endif
