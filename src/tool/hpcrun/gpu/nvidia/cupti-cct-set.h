#ifndef cupti_cct_set_h
#define cupti_cct_set_h


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
typedef struct cupti_cct_set_entry_s cupti_cct_set_entry_t;

cupti_cct_set_entry_t *
cupti_cct_set_lookup
(
 cct_node_t *cct
); 


void
cupti_cct_set_insert
(
 cct_node_t *cct
);


void
cupti_cct_set_clear
(
);


#endif


