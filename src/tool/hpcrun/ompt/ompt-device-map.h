#ifndef _hpctoolkit_ompt_device_map_h_
#define _hpctoolkit_ompt_device_map_h_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>
#include <ompt.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>


/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct ompt_device_map_entry_s ompt_device_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_device_map_entry_t *ompt_device_map_lookup(uint64_t id);

void ompt_device_map_insert(uint64_t device_id, ompt_device_t *ompt_device, char *type);

bool ompt_device_map_refcnt_update(uint64_t device_id, int val);

uint64_t ompt_device_map_entry_refcnt_get(ompt_device_map_entry_t *entry);

ompt_device_t *ompt_device_map_entry_device_get(ompt_device_map_entry_t *entry);

#endif
