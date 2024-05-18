// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_DEVICE_H__
#define __OMPT_DEVICE_H__

#include <stdbool.h>
#include "../cct/cct.h"

void
prepare_device
(
 void
);


//---------------------------------------------
// If a API is invoked by OMPT (TRUE/FALSE)
//---------------------------------------------

bool
ompt_runtime_status_get
(
 void
);


cct_node_t *
ompt_trace_node_get
(
 void
);


//-----------------------------------------------------------------------------
// NVIDIA GPU pc sampling support
//-----------------------------------------------------------------------------

void
ompt_pc_sampling_enable
(
 void
);


void
ompt_pc_sampling_disable
(
 void
);

//-----------------------------------------------------------------------------
// Use hpctoolkit callback/OMPT callback
//-----------------------------------------------------------------------------

void
ompt_external_subscriber_enable
(
 void
);


void
ompt_external_subscriber_disable
(
 void
);

#endif // _OMPT_INTERFACE_H_
