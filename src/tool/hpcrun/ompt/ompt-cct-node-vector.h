#ifndef _hpctoolkit_ompt_cct_node_vector_h_
#define _hpctoolkit_ompt_cct_node_vector_h_

/******************************************************************************
 * hpcrun customized structure for dynamic vector
 *****************************************************************************/

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>

typedef struct ompt_cct_node_vector_s ompt_cct_node_vector_t;

ompt_cct_node_vector_t *ompt_cct_node_vector_init();

void ompt_cct_node_vector_reserve(ompt_cct_node_vector_t *vector, uint64_t capacity);

void ompt_cct_node_vector_push_back(ompt_cct_node_vector_t *vector, cct_node_t *node);

cct_node_t *ompt_cct_node_vector_get(ompt_cct_node_vector_t *vector, uint64_t host_op_seq_id);

#endif
