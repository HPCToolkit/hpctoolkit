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
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef cupti_pc_sampling_api_h
#define cupti_pc_sampling_api_h

#include <cupti.h>
#include <cupti_pcsampling.h>
#include "../gpu-activity.h"

#include "cupti-ip-norm-map.h"

// Configures pc sampling frequency of a context
void
cupti_pc_sampling_config
(
 CUcontext context,
 int frequency
);


// XXX(Keren): continuous pc sampling only profiles a single context.
// Other concurrent contexts have to be disabled.
// Enables pc sampling for a context
void
cupti_pc_sampling_enable2
(
 CUcontext context
);

// Disables pc sampling for a context
void
cupti_pc_sampling_disable2
(
 CUcontext context
);


// Binds pc sampling API functions
int
cupti_pc_sampling_bind
(
);


// Gets the crc of a cubin
uint64_t
cupti_cubin_crc_get
(
 const void *cubin,
 uint32_t cubin_size
);


// Collects and attributes samples to the default range
void
cupti_pc_sampling_correlation_context_collect
(
 cct_node_t *cct_node,
 CUcontext context
);


// Collects and attributes samples to a given range under a given context
void
cupti_pc_sampling_range_context_collect
(
 uint32_t range_id,
 CUcontext context
);


// Collects and attributes samples to a range under the current context
void
cupti_pc_sampling_range_collect
(
 uint32_t range_id
);


// Frees pc sampling buffers used by the given context
void
cupti_pc_sampling_free
(
 CUcontext context
);


// Starts pc sampling of the given context
void
cupti_pc_sampling_start
(
 CUcontext context
);


// Stops pc sampling of the given context
void
cupti_pc_sampling_stop
(
 CUcontext context
);


// Returns whether pc sampling is turned on 
bool
cupti_pc_sampling_active
(
);

#endif
