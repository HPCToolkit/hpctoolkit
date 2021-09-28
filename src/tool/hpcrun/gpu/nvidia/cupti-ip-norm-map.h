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
  CUPTI_IP_NORM_MAP_EXIST = 0,
  CUPTI_IP_NORM_MAP_DUPLICATE = 0,
  CUPTI_IP_NORM_MAP_COUNT = 0,
} cupti_ip_norm_map_ret_t;


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup
(
 cct_node_t *cct
); 


void
cupti_ip_norm_map_insert
(
 cct_node_t *cct
);


void
cupti_ip_norm_map_clear
(
);


#endif


