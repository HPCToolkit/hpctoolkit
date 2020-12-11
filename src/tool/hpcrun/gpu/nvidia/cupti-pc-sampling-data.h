#ifndef cupti_pc_sampling_data_h
#define cupti_pc_sampling_data_h

#include <cupti.h>
#include <cupti_pcsampling.h>

typedef struct cupti_pc_sampling_data_t cupti_pc_sampling_data_t;

cupti_pc_sampling_data_t *
cupti_pc_sampling_data_produce
(
 size_t num_pcs,
 size_t num_stall_reasons
);


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
