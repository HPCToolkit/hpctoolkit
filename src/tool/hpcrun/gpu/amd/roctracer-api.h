//
// Created by tx7 on 8/15/19.
//

#ifndef HPCTOOLKIT_ROCTRACER_API_H
#define HPCTOOLKIT_ROCTRACER_API_H

#include <roctracer_hip.h>

void
roctracer_subscriber_callback
(
        uint32_t domain,
        uint32_t callback_id,
        const void* callback_data,
        void* arg
);

void
roctracer_buffer_completion_callback
(
        const char* begin,
        const char* end,
        void* arg
);

void
roctracer_init
(

);

void
roctracer_fini
(

);

int
roctracer_bind
(
  void
);

#endif //HPCTOOLKIT_ROCTRACER_API_H
