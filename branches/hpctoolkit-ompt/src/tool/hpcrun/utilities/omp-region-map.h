#ifndef _hpctoolkit_omp_region_map_h_
#define _hpctoolkit_omp_region_map_h_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct omp_region_map_entry_s omp_region_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

omp_region_map_entry_t *omp_region_map_lookup(uint64_t id);

void omp_region_map_insert(uint64_t region_id, cct_node_t *call_path);

bool omp_region_map_refcnt_update(uint64_t region_id, uint64_t val);

uint64_t omp_region_map_entry_refcnt_get(omp_region_map_entry_t *node);

void omp_region_map_entry_callpath_set(omp_region_map_entry_t *node, 
				       cct_node_t *call_path);

cct_node_t *omp_region_map_entry_callpath_get(omp_region_map_entry_t *node);

#endif
