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
// Copyright ((c)) 2002-2023, Rice University
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

#include "../cct/cct.h"

#include "../messages/errors.h"

#include "../../../lib/prof-lean/placeholders.h"
#include "gpu-op-placeholders.h"



//******************************************************************************
// macros
//******************************************************************************

#define SET_LOW_N_BITS(n, type) (~(((type) ~0) << n))


//******************************************************************************
// public data
//******************************************************************************

gpu_op_placeholder_flags_t gpu_op_placeholder_flags_none = 0;

gpu_op_placeholder_flags_t gpu_op_placeholder_flags_all =
  SET_LOW_N_BITS(gpu_placeholder_type_count, gpu_op_placeholder_flags_t);


//******************************************************************************
// private operations
//******************************************************************************

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
  switch(type) {
  #define CASE(N) case gpu_placeholder_type_##N: return get_placeholder_norm(hpcrun_placeholder_gpu_##N);
  CASE(copy)
  CASE(copyin)
  CASE(copyout)
  CASE(alloc)
  CASE(delete)
  CASE(kernel)
  CASE(memset)
  CASE(sync)
  CASE(trace)
  #undef CASE
  case gpu_placeholder_type_count:
    break;
  }
  assert(false && "Invalid GPU placeholder type!");
  hpcrun_terminate();
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
      node = hpcrun_cct_insert_ip_norm(api_node, gpu_op_placeholder_ip(i), true);
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
