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
// Copyright ((c)) 2002-2021, Rice University
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

#ifndef gpu_trace_h
#define gpu_trace_h

#include <stdbool.h>


//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// forward declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;

typedef struct thread_data_t thread_data_t;

typedef struct gpu_trace_channel_t gpu_trace_channel_t;

typedef struct gpu_trace_t gpu_trace_t;

typedef struct gpu_trace_item_t gpu_trace_item_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef void (*gpu_trace_fn_t)
(
 gpu_trace_t *trace,
 gpu_trace_item_t *ti
);


typedef struct gpu_tag_t {
  uint32_t device_id;
  uint32_t context_id;
  uint32_t stream_id;
}gpu_tag_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_trace_init
(
 void
);


void
gpu_trace_fini
(
 void *arg,
 int how
);


void *
gpu_trace_record
(
 void *args
);


gpu_trace_t *
gpu_trace_create
(
 uint32_t device_id,
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_trace_produce
(
 gpu_trace_t *t,
 gpu_trace_item_t *ti
);


void
gpu_trace_signal_consumer
(
 gpu_trace_t *t
);


void
consume_one_trace_item
(
thread_data_t* td,
cct_node_t *call_path,
uint64_t start_time,
uint64_t end_time
);


thread_data_t *
gpu_trace_stream_acquire
(
 gpu_tag_t tag
);


void
gpu_trace_stream_release
(
 gpu_trace_channel_t *channel
);




#endif 
