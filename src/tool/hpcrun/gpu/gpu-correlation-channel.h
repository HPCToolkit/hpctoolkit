// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef gpu_correlation_channel_h
#define gpu_correlation_channel_h


//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-correlation.h"
#include "gpu-channel-common.h"

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t gpu_correlation_channel_t;

typedef struct gpu_op_ccts_t gpu_op_ccts_t;



//******************************************************************************
// interface operations 
//******************************************************************************

// produce into the first channel that my thread created
void
gpu_correlation_channel_produce
(
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_ccts,
 uint64_t cpu_submit_time
);

// produce into a specified channel (with idx) that my thread created
// when idx == 0, this function is equivalent to gpu_correlation_channel_produce
void
gpu_correlation_channel_produce_with_idx
(
 int idx,
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_ccts,
 uint64_t cpu_submit_time
);

// consume from a channel that another thread created
void
gpu_correlation_channel_consume
(
 gpu_correlation_channel_t *channel
);



#endif
