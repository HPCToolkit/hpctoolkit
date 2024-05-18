// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef ompt_activity_translate_h
#define ompt_activity_translate_h


//******************************************************************************
// OpenMP includes
//******************************************************************************

#include "../../../ompt/omp-tools.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t;
typedef struct cct_node_t cct_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
ompt_activity_translate
(
 gpu_activity_t *entry,
 ompt_record_ompt_t *record,
 uint64_t *cid_ptr
);



#endif
