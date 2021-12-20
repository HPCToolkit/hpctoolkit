#ifndef cupti_context_map_h
#define cupti_context_map_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <cuda.h>

#include "cupti-pc-sampling-data.h"

//*****************************************************************************
// type definitions 
//*****************************************************************************

typedef struct cupti_context_map_entry_s cupti_context_map_entry_t;

typedef void (*cupti_context_process_fn_t)
(
 CUcontext context,
 void *args
);

typedef struct cupti_context_process_args_t {
  cupti_context_process_fn_t fn;
  void *args;
} cupti_context_process_args_t;

//*****************************************************************************
// interface operations
//*****************************************************************************

cupti_context_map_entry_t *
cupti_context_map_lookup
(
 CUcontext context
); 


void
cupti_context_map_init
(
 CUcontext context
);


void
cupti_context_map_pc_sampling_insert
(
 CUcontext context,
 size_t num_stall_reasons,
 uint32_t *stall_reason_index,
 char **stall_reason_names,
 size_t num_buffer_pcs,
 cupti_pc_sampling_data_t *buffer_pc
);


void
cupti_context_map_delete
(
 CUcontext context
);


cupti_pc_sampling_data_t *
cupti_context_map_entry_pc_sampling_data_get
(
 cupti_context_map_entry_t *entry
);


void
cupti_context_map_process
(
 cupti_context_process_fn_t fn,
 void *args
);


size_t
cupti_context_map_entry_num_stall_reasons_get
(
 cupti_context_map_entry_t *entry
);


uint32_t *
cupti_context_map_entry_stall_reason_index_get
(
 cupti_context_map_entry_t *entry
);


char **
cupti_context_map_entry_stall_reason_names_get
(
 cupti_context_map_entry_t *entry
);

#endif
