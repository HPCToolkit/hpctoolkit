// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef cupti_gpu_api_h
#define cupti_gpu_api_h

//******************************************************************************
// nvidia includes
//******************************************************************************

#include <cupti_activity.h>



//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_activity_process
(
 CUpti_Activity *cupti_activity
);


#endif
