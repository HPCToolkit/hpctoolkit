#ifndef gpu_instrumentation_simdgroup_map_h
#define gpu_instrumentation_simdgroup_map_h

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>
#include <gtpin.h>



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct simdgroup_map_entry_t simdgroup_map_entry_t;

simdgroup_map_entry_t*
simdgroup_map_lookup
(
 uint64_t simdgroup_id
);


simdgroup_map_entry_t*
simdgroup_map_insert
(
 uint64_t simdgroup_id,
 bool maskCtrl,
 uint32_t execMask,
 GenPredArgs predArgs
);


void
simdgroup_map_delete
(
 uint64_t simdgroup_id
);


uint32_t
simdgroup_entry_getExecMask
(
 simdgroup_map_entry_t *entry
);


bool
simdgroup_entry_getMaskCtrl
(
 simdgroup_map_entry_t *entry
);


GenPredArgs
simdgroup_entry_getPredArgs
(
 simdgroup_map_entry_t *entry
);

#endif
