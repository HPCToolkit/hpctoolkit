#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_CONTEXT_MAP_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_CONTEXT_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <cuda.h>
#include <sanitizer.h>

#include <gpu-patch.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct sanitizer_context_map_entry_s sanitizer_context_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_context_map_entry_t *
sanitizer_context_map_lookup
(
 CUcontext context
);


sanitizer_context_map_entry_t *
sanitizer_context_map_init
(
 CUcontext context
);


void
sanitizer_context_map_delete
(
 CUcontext context
);


void
sanitizer_context_map_insert
(
 CUcontext context,
 CUstream stream
);


void
sanitizer_context_map_analysis_function_update
(
 CUcontext context,
 CUfunction function
);


void
sanitizer_context_map_context_lock
(
 CUcontext context
);


void
sanitizer_context_map_context_unlock
(
 CUcontext context
);


void
sanitizer_context_map_stream_lock
(
 CUcontext context,
 CUstream stream
);


void
sanitizer_context_map_stream_unlock
(
 CUcontext context,
 CUstream stream
);


void
sanitizer_context_map_priority_stream_handle_update
(
 CUcontext context,
 Sanitizer_StreamHandle priority_stream_handle
);


void
sanitizer_context_map_kernel_stream_handle_update
(
 CUcontext context,
 Sanitizer_StreamHandle kernel_stream_handle
);


void
sanitizer_context_map_buffer_device_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_device
);


void
sanitizer_context_map_buffer_addr_read_device_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_addr_read_device
);


void
sanitizer_context_map_buffer_addr_write_device_update
(
 CUcontext context,
 gpu_patch_buffer_t *buffer_addr_write_device
);


void
sanitizer_context_map_addr_dict_device_update
(
 CUcontext context,
 gpu_patch_aux_address_dict_t *addr_dict_device
 );


CUstream
sanitizer_context_map_entry_priority_stream_get
(
 sanitizer_context_map_entry_t *entry
);


CUstream
sanitizer_context_map_entry_kernel_stream_get
(
 sanitizer_context_map_entry_t *entry
);


Sanitizer_StreamHandle
sanitizer_context_map_entry_priority_stream_handle_get
(
 sanitizer_context_map_entry_t *entry
);


Sanitizer_StreamHandle
sanitizer_context_map_entry_kernel_stream_handle_get
(
 sanitizer_context_map_entry_t *entry
);


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_device_get
(
 sanitizer_context_map_entry_t *entry
);


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_addr_read_device_get
(
 sanitizer_context_map_entry_t *entry
);


gpu_patch_buffer_t *
sanitizer_context_map_entry_buffer_addr_write_device_get
(
 sanitizer_context_map_entry_t *entry
);


gpu_patch_aux_address_dict_t *
sanitizer_context_map_entry_addr_dict_device_get
(
 sanitizer_context_map_entry_t *entry
);


CUfunction
sanitizer_context_map_entry_analysis_function_get
(
 sanitizer_context_map_entry_t *entry
);


#endif
