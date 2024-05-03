// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef ompt_gpu_api_h
#define ompt_gpu_api_h



//******************************************************************************
// OpenMP includes
//******************************************************************************

#include "../../../ompt/omp-tools.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
ompt_buffer_completion_notify
(
 void
);


void
ompt_activity_process
(
 ompt_record_ompt_t *record
);



#endif
