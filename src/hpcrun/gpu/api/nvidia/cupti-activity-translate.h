// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef cupti_activity_translate_h
#define cupti_activity_translate_h


//******************************************************************************
// nvidia includes
//******************************************************************************

#define _GNU_SOURCE

#include <cupti_activity.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t;
typedef struct cct_node_t cct_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

bool
cupti_activity_translate
(
 gpu_activity_t *entry,
 CUpti_Activity *activity,
 uint64_t *correlation_id
);



#endif
