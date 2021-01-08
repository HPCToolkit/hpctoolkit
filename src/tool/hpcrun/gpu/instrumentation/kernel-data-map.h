#ifndef gpu_instrumentation_kernel_data_map_h
#define gpu_instrumentation_kernel_data_map_h

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>
#include "kernel-data.h"

//******************************************************************************
// interface operations
//******************************************************************************

typedef struct kernel_data_map_entry_t kernel_data_map_entry_t;

kernel_data_map_entry_t*
kernel_data_map_lookup
(
 uint64_t kernel_id
);


void
kernel_data_map_insert
(
 uint64_t kernel_id,
 kernel_data_t kernel_data
);


void
kernel_data_map_delete
(
 uint64_t kernel_id
);


kernel_data_t
kernel_data_map_entry_kernel_data_get
(
 kernel_data_map_entry_t *entry
);

#endif
