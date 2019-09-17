//
// Created by tx7 on 8/16/19.
//

#include "hip-state-placeholders.h"

#include <lib/prof-lean/placeholders.h>
#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>

hip_placeholders_t hip_placeholders;

void
hip_memcpy
(
        void
)
{
}


void
hip_memalloc
(
        void
)
{
}


void
hip_kernel
(
        void
)
{
}


void
hip_free
(
        void
)
{
}

void
hip_sync
(
        void
)
{
}



void
hip_init_placeholders
(
)
{
    init_placeholder(&hip_placeholders.hip_memcpy_state, &hip_memcpy);
    init_placeholder(&hip_placeholders.hip_memalloc_state, &hip_memalloc);
    init_placeholder(&hip_placeholders.hip_kernel_state, &hip_kernel);
    init_placeholder(&hip_placeholders.hip_free_state, &hip_free);
    init_placeholder(&hip_placeholders.hip_sync_state, &hip_sync);
}
