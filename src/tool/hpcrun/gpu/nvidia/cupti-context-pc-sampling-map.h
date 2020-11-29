#ifndef cupti_context_pc_sampling_map_h
#define cupti_context_pc_sampling_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <cupti.h>
#include <cupti_pcsampling.h>


//*****************************************************************************
// type definitions 
//*****************************************************************************

typedef struct cupti_context_pc_sampling_map_entry_s cupti_context_pc_sampling_map_entry_t;

typedef void (*cupti_context_pc_sampling_flush_fn_t)
(
 CUcontext context
);

//*****************************************************************************
// interface operations
//*****************************************************************************

cupti_context_pc_sampling_map_entry_t *
cupti_context_pc_sampling_map_lookup
(
 CUcontext context
); 


void
cupti_context_pc_sampling_map_insert
(
 CUcontext context,
 size_t num_stall_reasons,
 uint32_t *stall_reason_index,
 char **stall_reason_names,
 size_t buffer_pc_num,
 CUpti_PCSamplingData *buffer_pc,
 size_t user_buffer_pc_num,
 CUpti_PCSamplingData *user_buffer_pc
);


void
cupti_context_pc_sampling_map_delete
(
 CUcontext context
);


void
cupti_context_pc_sampling_map_delete
(
 CUcontext context
);


CUpti_PCSamplingData *
cupti_context_pc_sampling_map_entry_user_buffer_pc_get
(
 cupti_context_pc_sampling_map_entry_t *entry
);


CUpti_PCSamplingData *
cupti_context_pc_sampling_map_entry_buffer_pc_get
(
 cupti_context_pc_sampling_map_entry_t *entry
);


void
cupti_context_pc_sampling_flush
(
 CUcontext context
);


void
cupti_context_pc_sampling_map_flush
(
 cupti_context_pc_sampling_flush_fn_t fn
);


size_t
cupti_context_pc_sampling_map_entry_num_stall_reasons_get
(
 cupti_context_pc_sampling_map_entry_t *entry
);


uint32_t *
cupti_context_pc_sampling_map_entry_stall_reason_index_get
(
 cupti_context_pc_sampling_map_entry_t *entry
);


char **
cupti_context_pc_sampling_map_entry_stall_reason_names_get
(
 cupti_context_pc_sampling_map_entry_t *entry
);

#endif
