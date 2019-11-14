//
// Created by tx7 on 8/16/19.
//

#ifndef HPCTOOLKIT_HIP_STATE_PLACEHOLDERS_H
#define HPCTOOLKIT_HIP_STATE_PLACEHOLDERS_H

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/sample-sources/gpu/placeholders.h>




typedef struct {
    placeholder_t hip_none_state;
    placeholder_t hip_memcpy_state;
    placeholder_t hip_memalloc_state;
    placeholder_t hip_kernel_state;
    placeholder_t hip_free_state;
    placeholder_t hip_sync_state;
    placeholder_t hip_memset_state;
} hip_placeholders_t;

extern hip_placeholders_t hip_placeholders;

void
hip_init_placeholders
(
);

#endif //HPCTOOLKIT_HIP_STATE_PLACEHOLDERS_H
