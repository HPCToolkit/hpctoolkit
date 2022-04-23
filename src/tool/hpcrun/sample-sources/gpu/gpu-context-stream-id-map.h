// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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

//
// Created by ax4 on 8/1/19.
//

#ifndef HPCTOOLKIT_CUPTI_CONTEXT_STREAM_ID_MAP_H
#define HPCTOOLKIT_CUPTI_CONTEXT_STREAM_ID_MAP_H

#include "tool/hpcrun/cct/cct.h"
#include "tool/hpcrun/threadmgr.h"
#include "trace.h"

#include "lib/prof-lean/producer_wfq.h"
#include "lib/prof-lean/splay-macros.h"
#include "lib/prof-lean/stdatomic.h"

#include <monitor.h>
#include <pthread.h>

typedef struct stream_activity_data_s {
  cct_node_t* node;
  uint64_t start;
  uint64_t end;
} stream_activity_data_t;

typedef struct {
  producer_wfq_element_ptr_t next;
  stream_activity_data_t activity_data;
} typed_producer_wfq_elem(stream_activity_data_t);

typedef typed_producer_wfq_elem(stream_activity_data_t) stream_activity_data_elem;
typedef producer_wfq_t typed_producer_wfq(stream_activity_data_t);
typedef typed_producer_wfq(stream_activity_data_t) stream_activity_data_producer_wfq;

typed_producer_wfq_declare(stream_activity_data_t)
#define stream_activity_data_producer_wfq_enqueue typed_producer_wfq_enqueue(stream_activity_data_t)
#define stream_activity_data_producer_wfq_dequeue typed_producer_wfq_dequeue(stream_activity_data_t)

    typedef struct stream_thread_data_s {
  stream_activity_data_t_producer_wfq_t* wfq;
  pthread_cond_t* cond;
  pthread_mutex_t* mutex;
} stream_thread_data_t;

typedef struct cupti_context_stream_id_map_entry_s {
  uint64_t context_stream_id;
  stream_activity_data_t_producer_wfq_t* wfq;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
  pthread_t thread;
  struct cupti_context_stream_id_map_entry_s* left;
  struct cupti_context_stream_id_map_entry_s* right;
} cupti_context_stream_id_map_entry_t;

cupti_context_stream_id_map_entry_t*
cupti_context_stream_map_lookup(uint32_t context_id, uint32_t stream_id);

void cupti_context_stream_map_insert(uint32_t context_id, uint32_t stream_id);

void cupti_context_stream_map_delete(uint32_t context_id, uint32_t stream_id);

producer_wfq_t* cupti_context_stream_map_wfq_get(cupti_context_stream_id_map_entry_t* stream_entry);

pthread_cond_t*
cupti_context_stream_map_cond_get(cupti_context_stream_id_map_entry_t* stream_entry);

void enqueue_stream_data_activity(
    uint32_t context_id, uint32_t stream_id, uint64_t start, uint64_t end, cct_node_t* cct_node);

void signal_all_streams(

);

#endif  // HPCTOOLKIT_CUPTI_STREAM_ID_MAP_H
