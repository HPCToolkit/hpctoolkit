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

#ifndef gpu_operation_item_h
#define gpu_operation_item_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stacks.h>
#include <hpcrun/utilities/ip-normalized.h>

#include "gpu-activity.h"



//******************************************************************************
// forward declarations
//******************************************************************************

typedef struct gpu_operation_channel_t gpu_operation_channel_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_operation_item_t {
  s_element_t next;
  gpu_activity_channel_t *channel;
  gpu_activity_t activity;
  atomic_int *pending_operations;
  atomic_bool *flush;
} gpu_operation_item_t;


typedef void (*gpu_operation_channel_fn_t)
(
 gpu_operation_channel_t *channel
);

typedef void (*gpu_operation_item_fn_t)
(
 gpu_operation_item_t *it
);

//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_operation_item_consume
(
 gpu_operation_item_fn_t ap_fn,
 gpu_operation_item_t *it
);


gpu_operation_item_t *
gpu_operation_item_alloc
(
 gpu_operation_channel_t *channel
);


void
gpu_operation_item_free
(
 gpu_operation_channel_t *channel,
 gpu_operation_item_t *a
);


void
gpu_operation_item_dump
(
 gpu_operation_item_t *it
);



#endif
