/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include "cct-node-vector.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cct_node_vector_s {
  cct_node_t **nodes;
  uint64_t size;
  uint64_t capacity;
}; 


cct_node_vector_t *cct_node_vector_init()
{
  cct_node_vector_t *vector = (cct_node_vector_t *)hpcrun_malloc(sizeof(cct_node_vector_t));
  vector->size = 0;
  vector->capacity = 0;
  cct_node_vector_reserve(vector, 32);
  return vector;
}


void cct_node_vector_reserve(cct_node_vector_t *vector, uint64_t capacity)
{
  // FIXME(keren): free memory
  if (capacity > vector->capacity) {
    cct_node_t **new_nodes = (cct_node_t **)hpcrun_malloc(sizeof(cct_node_t *) * (capacity));
    size_t i = 0;
    for (; i < vector->size; ++i) {
      new_nodes[i] = vector->nodes[i];
    }
    vector->nodes = new_nodes;
  }
  vector->capacity = capacity;
}


void cct_node_vector_push_back(cct_node_vector_t *vector, cct_node_t *node)
{
  if (vector->size == vector->capacity) {
    cct_node_vector_reserve(vector, vector->capacity * 2);
  }
  vector->nodes[vector->size] = node;
  vector->size++;
}


cct_node_t *cct_node_vector_get(cct_node_vector_t *vector, uint64_t index)
{
  if (index < vector->size) {
    return vector->nodes[index];
  } else {
    // out of the bound
    return NULL;
  }
}
