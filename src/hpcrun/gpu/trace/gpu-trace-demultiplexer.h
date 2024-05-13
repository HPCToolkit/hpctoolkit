// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef gpu_trace_demultiplexer_h
#define gpu_trace_demultiplexer_h

//******************************************************************************
// system includes
//******************************************************************************

#include <pthread.h>




//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-trace-channel.h"


void
gpu_trace_demultiplexer_push
(
 gpu_trace_channel_t *trace_channel
);


void
gpu_trace_demultiplexer_notify
(
 void
);


#endif
