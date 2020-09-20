#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_OP_MAP_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_OP_MAP_H_

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

typedef struct sanitizer_op_map_entry_s sanitizer_op_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_op_map_entry_t *
sanitizer_op_map_lookup
(
 int32_t persistent_id
);


sanitizer_op_map_entry_t *
sanitizer_op_map_init
(
 int32_t persistent_id,
 cct_node_t *op
);


void
sanitizer_op_map_delete
(
 int32_t persistent_id
);


cct_node_t *
sanitizer_op_map_op_get
(
 sanitizer_op_map_entry_t *
);


#endif
