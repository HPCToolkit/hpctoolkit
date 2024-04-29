#ifndef CUDA_KERNEL_DATA_MAP_H
#define CUDA_KERNEL_DATA_MAP_H

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct cuda_kernel_data_map_entry_value_t {
  uint32_t device_id;
  uint64_t start;
  uint64_t end;
} cuda_kernel_data_map_entry_value_t;

typedef struct cuda_kernel_data_map_entry_t cuda_kernel_data_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

cuda_kernel_data_map_entry_value_t *
cuda_kernel_data_map_lookup
(
 uint64_t gpu_correlation_id
);


bool
cuda_kernel_data_map_insert
(
 uint64_t gpu_correlation_id,
 cuda_kernel_data_map_entry_value_t value
);


bool
cuda_kernel_data_map_delete
(
 uint64_t gpu_correlation_id
);


#endif
