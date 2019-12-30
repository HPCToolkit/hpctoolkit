#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_STREAM_MAP_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_STREAM_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <cuda.h>


/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct sanitizer_stream_map_entry_s sanitizer_stream_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_stream_map_entry_t *
sanitizer_stream_map_lookup
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


void
sanitizer_stream_map_insert
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


void
sanitizer_stream_map_delete
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


void
sanitizer_stream_map_stream_lock
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


void
sanitizer_stream_map_stream_unlock
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


sanitizer_stream_map_entry_t *
sanitizer_stream_map_entry_new
(
 CUstream stream
);

#endif

