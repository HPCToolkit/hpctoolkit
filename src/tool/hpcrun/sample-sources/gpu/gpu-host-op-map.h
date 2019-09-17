//
// Created by user on 19.8.2019..
//

#ifndef HPCTOOLKIT_GPU_HOST_OP_MAP_H
#define HPCTOOLKIT_GPU_HOST_OP_MAP_H

#include <stdint.h>
#include <hpcrun/cct/cct.h>
#include "gpu-record.h"

typedef struct gpu_host_op_map_entry_s {
    uint64_t host_op_id;
    int samples;
    int total_samples;
    gpu_record_t *record;
    cct_node_t *host_op_node;
    entry_data_t *data;
    struct gpu_host_op_map_entry_s *left;
    struct gpu_host_op_map_entry_s *right;
} gpu_host_op_map_entry_t;

gpu_host_op_map_entry_t *
gpu_host_op_map_lookup
        (
                uint64_t id
        );


void
gpu_host_op_map_insert
        (
                uint64_t host_op_id,
                cct_node_t *host_op_node,
                gpu_record_t *record,
                entry_data_t *data;
        );

// samples == total_samples remove the node and return false
bool
gpu_host_op_map_samples_increase
        (
                uint64_t host_op_id,
                int samples
        );

// samples == total_samples remove the node and return false
bool
gpu_host_op_map_total_samples_update
        (
                uint64_t host_op_id,
                int total_samples
        );


void
gpu_host_op_map_delete
        (
                uint64_t host_op_id
        );


cct_node_t *
gpu_host_op_map_entry_host_op_node_get
        (
                gpu_host_op_map_entry_t *entry
        );


gpu_record_t *
gpu_host_op_map_entry_record_get
        (
                gpu_host_op_map_entry_t *entry
        );

entry_data_t *
gpu_host_op_map_entry_data_get
(
        gpu_host_op_map_entry_t *entry
);

#endif //HPCTOOLKIT_GPU_HOST_OP_MAP_H
