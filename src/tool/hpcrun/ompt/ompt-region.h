#ifndef __ompt_region_h
#define __ompt_region_h

#include <ompt.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

ompt_region_data_t* ompt_region_data_new(uint64_t region_id, cct_node_t *call_path);

bool ompt_region_data_refcnt_update(ompt_region_data_t* region_data, int val);

uint64_t ompt_region_data_refcnt_get(ompt_region_data_t* region_data);

void ompt_parallel_region_register_callbacks(ompt_set_callback_t ompt_set_callback_fn);

#endif
