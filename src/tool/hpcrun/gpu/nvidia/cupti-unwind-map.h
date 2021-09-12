#ifndef cupti_unwind_map_h
#define cupti_unwind_map_h

#include <stdint.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/utilities/ip-normalized.h>


typedef struct unwind_key_t {
  size_t stack_length;
  ip_normalized_t function_id;
  cct_node_t *prev;
} unwind_key_t;


bool
cupti_unwind_map_insert
(
 unwind_key_t key,
 cct_node_t *cct_node
);


cct_node_t *
cupti_unwind_map_cct_node_lookup
(
 unwind_key_t key
);

#endif
