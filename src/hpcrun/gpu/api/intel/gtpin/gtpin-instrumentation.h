// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   gtpin-instrumentation.h
//
// Purpose:
//   define API for instrumenting Intel GPU binaries with GTPin
//
//***************************************************************************

#ifndef gtpin_instrumentation_h
#define gtpin_instrumentation_h

//*****************************************************************************
// system include files
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include "../../../../cct/cct.h"
#include "../../common/gpu-instrumentation.h"
#include "../../../activity/gpu-op-placeholders.h"
#include "../../../../utilities/ip-normalized.h"

#include "gtpin-hpcrun-api.h"


#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
// interface functions
//*****************************************************************************

void
gtpin_instrumentation_options
(
  gpu_instrumentation_t *
);


void
gtpin_produce_runtime_callstack
(
  gpu_op_ccts_t *
);


void
gtpin_process_block_instructions
(
  cct_node_t *
);


void
gtpin_hpcrun_api_set
(
  gtpin_hpcrun_api_t *
);


ip_normalized_t
gtpin_lookup_kernel_ip
(
  const char *kernel_name
);



#ifdef __cplusplus
}  // extern "C"
#endif

#endif
