#ifndef _hpctoolkit_ompt_stop_map_h_
#define _hpctoolkit_ompt_stop_map_h_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct ompt_stop_map_entry_s ompt_stop_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_stop_map_entry_t *ompt_stop_map_lookup(bool *flag);

void ompt_stop_map_insert(bool *flag);

bool ompt_stop_map_refcnt_update(bool *flag, int val);

uint64_t ompt_stop_map_entry_refcnt_get(ompt_stop_map_entry_t *entry);

#endif
