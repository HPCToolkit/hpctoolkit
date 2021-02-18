#ifndef gpu_blame_kernel_cleanup_map_h_
#define gpu_blame_kernel_cleanup_map_h_

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>                           // uint64_t



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-opencl.h>      // cl_event



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct kernel_id_t {
  long length;
  uint64_t *id;
} kernel_id_t;


// kernel_cleanup_data_t maintains platform-language specific information.
// Can be used for cleanup purposes in sync_epilogue callback
typedef struct kernel_cleanup_data_t {
  // platform/language specific variables
  union {
    cl_event *event;
  };
} kernel_cleanup_data_t;



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct kernel_cleanup_map_entry_t kernel_cleanup_map_entry_t;

kernel_cleanup_map_entry_t*
kernel_cleanup_map_lookup
(
 uint64_t kernel_id
);


void
kernel_cleanup_map_insert
(
 uint64_t kernel_id,
 kernel_cleanup_data_t *data
);


void
kernel_cleanup_map_delete
(
 uint64_t kernel_id
);


kernel_cleanup_data_t*
kernel_cleanup_map_entry_data_get
(
 kernel_cleanup_map_entry_t *entry
);

#endif		// gpu_blame_kernel_cleanup_map_h_
