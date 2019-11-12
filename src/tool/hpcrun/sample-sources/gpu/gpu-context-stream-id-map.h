//
// Created by ax4 on 8/1/19.
//

#ifndef HPCTOOLKIT_CUPTI_CONTEXT_STREAM_ID_MAP_H
#define HPCTOOLKIT_CUPTI_CONTEXT_STREAM_ID_MAP_H

#include <lib/prof-lean/producer_wfq.h>
#include <pthread.h>
#include <tool/hpcrun/cct/cct.h>
#include <lib/prof-lean/splay-macros.h>
#include <monitor.h>
#include "trace.h"
#include <lib/prof-lean/stdatomic.h>
#include <tool/hpcrun/threadmgr.h>

typedef struct stream_activity_data_s {
    cct_node_t *node;
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
    stream_activity_data_t_producer_wfq_t *wfq;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
} stream_thread_data_t;


typedef struct cupti_context_stream_id_map_entry_s {
    uint64_t context_stream_id;
    stream_activity_data_t_producer_wfq_t *wfq;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    pthread_t thread;
    struct cupti_context_stream_id_map_entry_s *left;
    struct cupti_context_stream_id_map_entry_s *right;
} cupti_context_stream_id_map_entry_t;

cupti_context_stream_id_map_entry_t *
cupti_context_stream_map_lookup
(
     uint32_t context_id,
     uint32_t stream_id
);

void
cupti_context_stream_map_insert
(
     uint32_t context_id,
     uint32_t stream_id
);

void
cupti_context_stream_map_delete
(
     uint32_t context_id,
     uint32_t stream_id
);

producer_wfq_t *
cupti_context_stream_map_wfq_get
(
   cupti_context_stream_id_map_entry_t *stream_entry
);

pthread_cond_t*
cupti_context_stream_map_cond_get
(
   cupti_context_stream_id_map_entry_t *stream_entry
);

void
enqueue_stream_data_activity
(
        uint32_t context_id,
        uint32_t stream_id,
        uint64_t start,
        uint64_t end,
        cct_node_t *cct_node
);

void
signal_all_streams
(

);

#endif //HPCTOOLKIT_CUPTI_STREAM_ID_MAP_H
