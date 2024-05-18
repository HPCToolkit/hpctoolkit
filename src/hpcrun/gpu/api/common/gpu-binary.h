// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_binary_h
#define gpu_binary_h


//******************************************************************************
// system include
//******************************************************************************

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


//******************************************************************************
// type declarations
//******************************************************************************

typedef enum {
  gpu_binary_kind_malformed = 0,
  gpu_binary_kind_unknown = 1,
  gpu_binary_kind_empty = 2,
  gpu_binary_kind_elf = 3,
  gpu_binary_kind_intel_patch_token = 4
} gpu_binary_kind_t;



//******************************************************************************
// interface operations
//******************************************************************************

gpu_binary_kind_t
gpu_binary_kind
(
 const char *mem_ptr,
 size_t mem_size
);


bool
gpu_binary_store
(
  const char *file_name,
  const void *binary,
  size_t binary_size
);

void
gpu_binary_path_generate
(
  const char *file_name,
  char *path,
  char *fullpath
);


// returns the loadmap id
uint32_t
gpu_binary_loadmap_insert
(
  const char *device_file,
  bool mark_used
);


bool
gpu_binary_save
(
 const char *mem_ptr,
 size_t mem_size,
 bool mark_used,
 uint32_t *loadmap_module_id
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
