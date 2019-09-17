//
// Created by user on 19.8.2019..
//

#ifndef HPCTOOLKIT_GPU_API_H
#define HPCTOOLKIT_GPU_API_H

#include "placeholders.h"
#include "gpu-record.h"

void
gpu_correlation_callback
(
        uint64_t id,
        placeholder_t state,
        entry_data_t *entry_data
);

#endif //HPCTOOLKIT_GPU_API_H
