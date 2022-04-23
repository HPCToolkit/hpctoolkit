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

#include "gpu-context-stream-id-map.h"

#include "stream-tracing.h"

extern atomic_ullong stream_counter;

cupti_context_stream_id_map_entry_t* cupti_context_stream_id_map_root = NULL;

typed_producer_wfq_impl(stream_activity_data_t)

    static uint64_t cupti_context_stream_id_generate(uint32_t context_id, uint32_t stream_id) {
  return ((uint64_t)context_id << 32) | (uint64_t)stream_id;
}

static cupti_context_stream_id_map_entry_t*
cupti_context_stream_id_map_entry_new(uint32_t context_id, uint32_t stream_id) {
  cupti_context_stream_id_map_entry_t* e;
  e = (cupti_context_stream_id_map_entry_t*)hpcrun_malloc_safe(
      sizeof(cupti_context_stream_id_map_entry_t));
  e->context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);
  e->wfq = (producer_wfq_t*)hpcrun_malloc_safe(sizeof(producer_wfq_t));
  //    pthread_mutex_init(&e->mutex, NULL);
  //    pthread_cond_init(&e->cond, NULL);
  producer_wfq_init(e->wfq);
  e->left = NULL;
  e->right = NULL;

  return e;
}

static cupti_context_stream_id_map_entry_t*
cupti_context_stream_id_map_splay(cupti_context_stream_id_map_entry_t* root, uint64_t key) {
  REGULAR_SPLAY_TREE(
      cupti_context_stream_id_map_entry_s, root, key, context_stream_id, left, right);
  return root;
}

static void cupti_context_stream_id_map_delete_root() {
  TMSG(
      DEFER_CTXT, "context_stream_id %d: delete",
      cupti_context_stream_id_map_root->context_stream_id);

  if (cupti_context_stream_id_map_root->left == NULL) {
    cupti_context_stream_id_map_root = cupti_context_stream_id_map_root->right;
  } else {
    cupti_context_stream_id_map_root->left = cupti_context_stream_id_map_splay(
        cupti_context_stream_id_map_root->left,
        cupti_context_stream_id_map_root->context_stream_id);
    cupti_context_stream_id_map_root->left->right = cupti_context_stream_id_map_root->right;
    cupti_context_stream_id_map_root = cupti_context_stream_id_map_root->left;
  }
}

cupti_context_stream_id_map_entry_t*
cupti_context_stream_map_lookup(uint32_t context_id, uint32_t stream_id) {
  uint64_t context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);

  cupti_context_stream_id_map_entry_t* result = NULL;

  cupti_context_stream_id_map_root =
      cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root, context_stream_id);
  if (cupti_context_stream_id_map_root
      && cupti_context_stream_id_map_root->context_stream_id == context_stream_id) {
    result = cupti_context_stream_id_map_root;
  }

  TMSG(DEFER_CTXT, "context_stream_id map lookup: id=0x%lx (record %p)", context_stream_id, result);
  return result;
}

void cupti_context_stream_id_map_insert(uint32_t context_id, uint32_t stream_id)

{
  uint64_t context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);

  cupti_context_stream_id_map_entry_t* entry =
      cupti_context_stream_id_map_entry_new(context_id, stream_id);

  TMSG(DEFER_CTXT, "context_stream_id map insert: id=0x%lx (record %p)", context_stream_id, entry);

  entry->left = entry->right = NULL;

  if (cupti_context_stream_id_map_root != NULL) {
    cupti_context_stream_id_map_root =
        cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root, context_stream_id);

    if (context_stream_id < cupti_context_stream_id_map_root->context_stream_id) {
      entry->left = cupti_context_stream_id_map_root->left;
      entry->right = cupti_context_stream_id_map_root;
      cupti_context_stream_id_map_root->left = NULL;
    } else if (context_stream_id > cupti_context_stream_id_map_root->context_stream_id) {
      entry->left = cupti_context_stream_id_map_root;
      entry->right = cupti_context_stream_id_map_root->right;
      cupti_context_stream_id_map_root->right = NULL;
    } else {
      // correlation_id already present: fatal error since a correlation_id
      //   should only be inserted once
      assert(0);
    }
  }
  cupti_context_stream_id_map_root = entry;
}

void cupti_context_stream_id_map_delete(uint32_t context_id, uint32_t stream_id) {
  uint64_t context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);
  cupti_context_stream_id_map_root =
      cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root, context_stream_id);

  if (cupti_context_stream_id_map_root
      && cupti_context_stream_id_map_root->context_stream_id == context_stream_id) {
    cupti_context_stream_id_map_delete_root();
  }
}

producer_wfq_t*
cupti_context_stream_map_wfq_get(cupti_context_stream_id_map_entry_t* stream_entry) {
  return stream_entry->wfq;
}

pthread_cond_t*
cupti_context_stream_map_cond_get(cupti_context_stream_id_map_entry_t* stream_entry) {
  return &stream_entry->cond;
}

void enqueue_stream_data_activity(
    uint32_t context_id, uint32_t stream_id, uint64_t start, uint64_t end, cct_node_t* cct_node) {
  cupti_context_stream_id_map_entry_t* entry =
      cupti_context_stream_map_lookup(context_id, stream_id);
  if (!entry) {
    cupti_context_stream_id_map_insert(context_id, stream_id);
    entry = cupti_context_stream_map_lookup(context_id, stream_id);  // new node will be root
    monitor_disable_new_threads();                                   // only once?
    stream_thread_data_t* data = hpcrun_malloc_safe(sizeof(stream_thread_data_t));
    data->cond = &entry->cond;
    data->wfq = entry->wfq;
    data->mutex = &entry->mutex;
    atomic_fetch_add(&stream_counter, 1);
    pthread_create(&entry->thread, NULL, stream_data_collecting, data);
    monitor_enable_new_threads();
  }
  stream_activity_data_elem* node =
      (stream_activity_data_elem*)hpcrun_malloc_safe(sizeof(stream_activity_data_elem));
  node->activity_data.node = cct_node;
  node->activity_data.start = start;
  node->activity_data.end = end;
  producer_wfq_ptr_set(&node->next, 0);
  stream_activity_data_producer_wfq* wfq = cupti_context_stream_map_wfq_get(entry);
  pthread_cond_t* cond = cupti_context_stream_map_cond_get(entry);
  pthread_mutex_t* mutex = &entry->mutex;
  stream_activity_data_t_producer_wfq_enqueue(wfq, node);
  pthread_cond_signal(cond);
}

static void splay_tree_traversal(cupti_context_stream_id_map_entry_t* root) {
  pthread_cond_signal(&root->cond);
  if (root->left)
    splay_tree_traversal(root->left);
  if (root->right)
    splay_tree_traversal(root->right);
}

void signal_all_streams() {
  if (cupti_context_stream_id_map_root) {
    splay_tree_traversal(cupti_context_stream_id_map_root);
  }
}
