#ifndef _HPCTOOLKIT_CUPTI_CONTEXT_MAP_H_
#define _HPCTOOLKIT_CUPTI_CONTEXT_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <cupti_activity.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_context_map_entry_s cupti_context_map_entry_t;


/******************************************************************************
 * interface operations
 *****************************************************************************/

size_t cupti_context_map_activity_num();

cupti_context_map_entry_t *cupti_context_map_lookup(CUcontext context);

void cupti_context_map_enable(CUcontext context, CUpti_ActivityKind activity);

void cupti_context_map_disable(CUcontext context, CUpti_ActivityKind activity);

bool cupti_context_map_refcnt_update(CUcontext context, int val);

uint64_t cupti_context_map_entry_refcnt_get(cupti_context_map_entry_t *entry);

bool cupti_context_map_entry_activity_get(cupti_context_map_entry_t *entry, CUpti_ActivityKind activity);

bool cupti_context_map_entry_activity_status_get(cupti_context_map_entry_t *entry, CUpti_ActivityKind activity);

#endif

