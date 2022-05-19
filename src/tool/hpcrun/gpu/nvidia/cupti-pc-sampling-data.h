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

#ifndef cupti_pc_sampling_data_h
#define cupti_pc_sampling_data_h

#include <cupti.h>
#include <cupti_pcsampling.h>

// cupti_pc_sampling_data_t is a wrapper of CUpti_PCSamplingData.
// It links multiple CUpti_PCSamplingData together to form a lock-free list.
typedef struct cupti_pc_sampling_data_t cupti_pc_sampling_data_t;

// Get pc sampling data from a lock-free queue.
// If the queue is empty, call malloc
cupti_pc_sampling_data_t *
cupti_pc_sampling_data_produce
(
 size_t num_pcs,
 size_t num_stall_reasons
);


// Release pc sampling data to a lock-free queue.
void
cupti_pc_sampling_data_free
(
 void *data
);


CUpti_PCSamplingData *
cupti_pc_sampling_buffer_pc_get
(
 cupti_pc_sampling_data_t *data
);

#endif
