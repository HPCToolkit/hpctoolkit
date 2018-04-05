#ifndef _HPCTOOLKIT_CUPTI_CALLSTACK_IGNORE_MAP_H_
#define _HPCTOOLKIT_CUPTI_CALLSTACK_IGNORE_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/loadmap.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_callstack_ignore_map_entry_s cupti_callstack_ignore_map_entry_t;


/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_callstack_ignore_map_entry_t *cupti_callstack_ignore_map_lookup(load_module_t *module);

void cupti_callstack_ignore_map_insert(load_module_t *module);

bool cupti_callstack_ignore_map_refcnt_update(load_module_t *module, int val);

uint64_t cupti_callstack_ignore_map_entry_refcnt_get(cupti_callstack_ignore_map_entry_t *entry);

bool cupti_callstack_ignore_map_ignore(load_module_t *module);

#endif

