#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_KERNEL_MAP_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_KERNEL_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct sanitizer_kernel_map_entry_s sanitizer_kernel_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_lookup
(
 cct_node_t *kernel
);


sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_init
(
 cct_node_t *kernel
);


void
sanitizer_kernel_map_delete
(
 cct_node_t *kernel
);


#endif
