// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef OPENCL_MEMORY_MANAGER_H
#define OPENCL_MEMORY_MANAGER_H



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bistack.h>
#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/gpu/gpu-activity.h>



//************************ Forward Declarations ******************************

typedef struct cct_node_t cct_node_t;



//******************************************************************************
// type declarations
//******************************************************************************

//This must be the same as gpu_activity_kind_t
//typedef enum {
//  GPU_ACTIVITY_KERNEL                     = 1,
//  GPU_ACTIVITY_MEMCPY                     = 2
//} gpu_activity_kind_t;


typedef struct opencl_object_channel_t opencl_object_channel_t;

typedef struct gpu_activity_channel_t gpu_activity_channel_t;

typedef struct cl_basic_callback_t {
  uint32_t correlation_id;
  gpu_activity_kind_t kind;
  gpu_memcpy_type_t type;
  cct_node_t *cct_node;
} cl_basic_callback_t;


typedef struct cl_kernel_callback_t {
  uint32_t correlation_id;
} cl_kernel_callback_t;


typedef struct cl_memcpy_callback_t {
  uint32_t correlation_id;
  gpu_memcpy_type_t type;
  bool fromHostToDevice;
  bool fromDeviceToHost;
  size_t size;
} cl_memcpy_callback_t;


typedef struct cl_memory_callback_t {
  uint32_t correlation_id;
  gpu_mem_type_t type;
  size_t size;
} cl_memory_callback_t;


typedef struct opencl_object_details_t {
  union {
    cl_kernel_callback_t ker_cb;
    cl_memcpy_callback_t cpy_cb;
    cl_memory_callback_t mem_cb;
  };
  gpu_activity_channel_t *initiator_channel;
  uint32_t context_id;
  uint32_t stream_id;
  uint32_t module_id;
  cct_node_t *cct_node;
  uint64_t submit_time;
} opencl_object_details_t;


typedef struct opencl_object_t {
  s_element_ptr_t next;
  opencl_object_channel_t *channel;
  gpu_activity_kind_t kind;
  bool internal_event;
  opencl_object_details_t details;
  atomic_int *pending_operations;
} opencl_object_t;



//******************************************************************************
// interface operations
//******************************************************************************

opencl_object_t *
opencl_malloc
(
 void
);


opencl_object_t *
opencl_malloc_kind
(
 gpu_activity_kind_t kind
);


void
opencl_free
(
 opencl_object_t *
);

#endif  //OPENCL_MEMORY_MANAGER_H
