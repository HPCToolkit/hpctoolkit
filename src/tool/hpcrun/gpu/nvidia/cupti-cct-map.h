#ifndef cupti_cct_map_h
#define cupti_cct_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>

//*****************************************************************************
// interface operations
//*****************************************************************************
//
typedef struct cupti_cct_map_entry_s cupti_cct_map_entry_t;

cupti_cct_map_entry_t *
cupti_cct_map_lookup
(
 cct_node_t *cct
); 


void
cupti_cct_map_insert
(
 cct_node_t *cct,
 cct_node_t *p1,
 cct_node_t *p2,
 cct_node_t *p3,
 cct_node_t *prev,
 ip_normalized_t function_id,
 size_t stack_length,
 size_t tool_depth,
 size_t api_depth,
 char *function_name,
 uint32_t grid_dim_x,
 uint32_t grid_dim_y,
 uint32_t grid_dim_z,
 uint32_t block_dim_x,
 uint32_t block_dim_y,
 uint32_t block_dim_z
);


void
cupti_cct_map_dump
(
);

size_t
cupti_cct_map_size_get
(
);

#endif

