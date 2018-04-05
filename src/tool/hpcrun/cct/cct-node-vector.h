#ifndef _HPCTOOLKIT_CCT_NODE_VECTOR_H_
#define _HPCTOOLKIT_CCT_NODE_VECTOR_H_

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

typedef struct cct_node_vector_s cct_node_vector_t;

cct_node_vector_t *cct_node_vector_init();

void cct_node_vector_reserve(cct_node_vector_t *vector, uint64_t capacity);

void cct_node_vector_push_back(cct_node_vector_t *vector, cct_node_t *node);

cct_node_t *cct_node_vector_get(cct_node_vector_t *vector, uint64_t index);

#endif
