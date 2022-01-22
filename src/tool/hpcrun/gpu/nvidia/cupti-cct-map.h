#ifndef cupti_cct_map_h
#define cupti_cct_map_h

#include <hpcrun/cct/cct.h>

typedef struct cupti_cct_map_entry_s cupti_cct_map_entry_t;

cupti_cct_map_entry_t *
cupti_cct_map_lookup(cct_node_t *cct);

void
cupti_cct_map_insert(cct_node_t *cct, uint32_t range_id);

void
cupti_cct_map_clear();

uint32_t
cupti_cct_map_entry_range_id_get(cupti_cct_map_entry_t *entry);

uint64_t
cupti_cct_map_entry_count_get(cupti_cct_map_entry_t *entry);

uint64_t
cupti_cct_map_entry_sampled_count_get(cupti_cct_map_entry_t *entry);

void
cupti_cct_map_entry_range_id_update(cupti_cct_map_entry_t *entry, uint32_t range_id);

void
cupti_cct_map_entry_count_increase(cupti_cct_map_entry_t *entry, uint64_t sampled_count, uint64_t count);

#endif
