// SPDX-FileCopyrightText: 2022-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef gpu_blame_kernel_cleanup_map_h_
#define gpu_blame_kernel_cleanup_map_h_

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>                           // uint64_t



//******************************************************************************
// local includes
//******************************************************************************

#include <CL/cl.h>



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
    cl_event event;
  };

  // only needed to maintain a list of free nodes
  struct kernel_cleanup_data_t *next;
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


kernel_cleanup_data_t*
kcd_alloc_helper
(
 void
);


void
kcd_free_helper
(
 kernel_cleanup_data_t *node
);

#endif          // gpu_blame_kernel_cleanup_map_h_
