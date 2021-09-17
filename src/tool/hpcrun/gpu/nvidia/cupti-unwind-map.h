#ifndef cupti_unwind_map_h
#define cupti_unwind_map_h

#include <stdint.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/utilities/ip-normalized.h>

typedef struct cupti_unwind_map_entry_s cupti_unwind_map_entry_t;


typedef struct unwind_key_t {
  size_t stack_length;
  ip_normalized_t function_id;
  cct_node_t *prev_prev_kernel;
  cct_node_t *prev_kernel;
  cct_node_t *prev_api;
} unwind_key_t;


bool
cupti_unwind_map_insert
(
 unwind_key_t key,
 cct_node_t *cct_node
);


bool
cupti_unwind_map_insert
(
 unwind_key_t key,
 cct_node_t *cct_node
);


cupti_unwind_map_entry_t *
cupti_unwind_map_lookup
(
 unwind_key_t key
);


cct_node_t *
cupti_unwind_map_entry_cct_node_get
(
 cupti_unwind_map_entry_t *entry
);


int
cupti_unwind_map_entry_backoff_get
(
 cupti_unwind_map_entry_t *entry
);


void
cupti_unwind_map_entry_backoff_update
(
 cupti_unwind_map_entry_t *entry,
 int backoff
);


void
cupti_unwind_map_entry_cct_node_update
(
 cupti_unwind_map_entry_t *entry,
 cct_node_t *cct_node
);

#endif
