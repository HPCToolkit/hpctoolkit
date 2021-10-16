#ifndef cupti_ip_norm_map_h
#define cupti_ip_norm_map_h


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
typedef struct cupti_ip_norm_map_entry_s cupti_ip_norm_map_entry_t;

typedef enum {
  CUPTI_IP_NORM_MAP_NOT_EXIST = 0,
  CUPTI_IP_NORM_MAP_EXIST = 1,
  CUPTI_IP_NORM_MAP_DUPLICATE = 2,
  CUPTI_IP_NORM_MAP_COUNT = 3,
} cupti_ip_norm_map_ret_t;


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup_thread
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
); 


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm,
 cct_node_t *cct
); 


void
cupti_ip_norm_map_insert_thread
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
);


void
cupti_ip_norm_map_insert
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm,
 cct_node_t *cct
);


void
cupti_ip_norm_map_merge
(
 cupti_ip_norm_map_entry_t **root,
 uint32_t prev_range_id,
 uint32_t range_id,
 uint32_t num_threads,
 bool sampled
);


void
cupti_ip_norm_map_merge_thread
(
 uint32_t prev_range_id,
 uint32_t range_id,
 uint32_t num_threads,
 bool sampled
);


void
cupti_ip_norm_map_clear
(
 cupti_ip_norm_map_entry_t **root
);


void
cupti_ip_norm_map_clear_thread
(
);

#endif


