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
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <pthread.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>

#include "gpu-op-placeholders.h"



//******************************************************************************
// macros
//******************************************************************************

#define SET_LOW_N_BITS(n, type) (~(((type) ~0) << n))



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_op_placeholders_t {
  placeholder_t ph[gpu_placeholder_type_count];
} gpu_op_placeholders_t;



//******************************************************************************
// public data
//******************************************************************************

gpu_op_placeholder_flags_t gpu_op_placeholder_flags_none = 0; 

gpu_op_placeholder_flags_t gpu_op_placeholder_flags_all =
  SET_LOW_N_BITS(gpu_placeholder_type_count, gpu_op_placeholder_flags_t);



//******************************************************************************
// local data
//******************************************************************************

static gpu_op_placeholders_t gpu_op_placeholders;


static pthread_once_t is_initialized = PTHREAD_ONCE_INIT;



//******************************************************************************
// placeholder functions
// 
// note: 
//   placeholder functions are not declared static so that the compiler 
//   doesn't eliminate their names from the symbol table. we need their
//   names in the symbol table to convert them into the appropriate placeholder 
//   strings in hpcprof
//******************************************************************************

void 
gpu_op_copy
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void 
gpu_op_copyin
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void 
gpu_op_copyout
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void 
gpu_op_alloc
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_delete
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_kernel
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_memset
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_sync
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_trace
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}



//******************************************************************************
// private operations
//******************************************************************************

static void 
gpu_op_placeholder_init
(
 gpu_placeholder_type_t type,
 void *pc
 )
{
  init_placeholder(&gpu_op_placeholders.ph[type], pc);
}


static void
gpu_op_placeholders_init
(
 void
)
{
  gpu_op_placeholder_init(gpu_placeholder_type_copy,    &gpu_op_copy);
  gpu_op_placeholder_init(gpu_placeholder_type_copyin,  &gpu_op_copyin);
  gpu_op_placeholder_init(gpu_placeholder_type_copyout, &gpu_op_copyout);
  gpu_op_placeholder_init(gpu_placeholder_type_alloc,   &gpu_op_alloc);
  gpu_op_placeholder_init(gpu_placeholder_type_delete,  &gpu_op_delete);
  gpu_op_placeholder_init(gpu_placeholder_type_kernel,  &gpu_op_kernel);
  gpu_op_placeholder_init(gpu_placeholder_type_memset,  &gpu_op_memset);
  gpu_op_placeholder_init(gpu_placeholder_type_sync,    &gpu_op_sync);
  gpu_op_placeholder_init(gpu_placeholder_type_trace,   &gpu_op_trace);
}


// debugging support
bool
gpu_op_ccts_empty
(
 gpu_op_ccts_t *gpu_op_ccts
)
{
  int i;
  for (i = 0; i < gpu_placeholder_type_count; i++) {
    if (gpu_op_ccts->ccts[i] != NULL) return false;
  }
  return true;
}



//******************************************************************************
// interface operations
//******************************************************************************

ip_normalized_t
gpu_op_placeholder_ip
(
 gpu_placeholder_type_t type
)
{
  pthread_once(&is_initialized, gpu_op_placeholders_init);

  return gpu_op_placeholders.ph[type].pc_norm;
}


cct_node_t *
gpu_op_ccts_get
(
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_placeholder_type_t type
)
{
  return gpu_op_ccts->ccts[type];
}


void
gpu_op_ccts_insert
(
 cct_node_t *api_node,
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_op_placeholder_flags_t flags
)
{
  int i;
  for (i = 0; i < gpu_placeholder_type_count; i++) {
    cct_node_t *node = NULL;

    if (flags & (1 << i)) {
      node = hpcrun_cct_insert_ip_norm(api_node, gpu_op_placeholder_ip(i));
    }

    gpu_op_ccts->ccts[i] = node;
  }
}


void
gpu_op_placeholder_flags_set
(
 gpu_op_placeholder_flags_t *flags,
 gpu_placeholder_type_t type
)
{
  *flags |= (1 << type);
}


bool
gpu_op_placeholder_flags_is_set
(
 gpu_op_placeholder_flags_t flags,
 gpu_placeholder_type_t type
)
{
  return (flags & (1 << type)) ? true : false;
}
