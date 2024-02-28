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
// Copyright ((c)) 2002-2024, Rice University
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
// Description:
//   Read fields from a ompt_record_ompt_t and assign to a
//   GPU-independent gpu_activity_t.
//
//   This interface is only used by the CUPTI GPU monitoring thread.
//   It is thread-safe as long as it does not access details structures
//   shared by worker threads.
//******************************************************************************

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../cct/cct.h"
#include "../../../cct/cct_addr.h"
#include "../../../utilities/ip-normalized.h"
#include "../../activity/gpu-activity.h"


#include "ompt-activity-translate.h"


//******************************************************************************
// macros
//******************************************************************************




//******************************************************************************
// private operations
//******************************************************************************

static void
convert_unknown
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  ga->kind = GPU_ACTIVITY_UNKNOWN;
  *cid_ptr = 0;
}


static void
convert_ptrop
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  ga->kind = GPU_ACTIVITY_UNKNOWN;
  *cid_ptr = 0;
}


static void
convert_target
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  ompt_record_target_t *t __attribute__((unused)) = &r->record.target;

  ga->kind = GPU_ACTIVITY_UNKNOWN;
  *cid_ptr = 0;
}


static void
convert_memory
(
  gpu_activity_t *ga,
  ompt_record_ompt_t *r,
  gpu_mem_op_t mem_op,
  uint64_t *cid_ptr
)
{
  ompt_record_target_data_op_t *d = &r->record.target_data_op;

  ga->kind = GPU_ACTIVITY_MEMORY;
  ga->details.memory.memKind = GPU_MEM_UNKNOWN;
  ga->details.memory.correlation_id = d->host_op_id;
  ga->details.memory.mem_op = mem_op;
  *cid_ptr = d->host_op_id;

  ga->details.memory.bytes = d->bytes;
}


static void
convert_alloc
(
  gpu_activity_t *ga,
  ompt_record_ompt_t *r,
  uint64_t *cid_ptr
)
{
  convert_memory(ga, r, GPU_MEM_OP_ALLOC, cid_ptr);
}


static void
convert_delete
(
  gpu_activity_t *ga,
  ompt_record_ompt_t *r,
  uint64_t *cid_ptr
)
{
  convert_memory(ga, r, GPU_MEM_OP_DELETE, cid_ptr);
}


static gpu_memcpy_type_t
convert_memcpy_type
(
 ompt_target_data_op_t kind
)
{
  switch (kind) {
  case ompt_target_data_transfer_to_device_async:
  case ompt_target_data_transfer_to_device:
    return GPU_MEMCPY_H2D;

  case ompt_target_data_transfer_from_device_async:
  case ompt_target_data_transfer_from_device:
    return GPU_MEMCPY_D2H;

  default:
    return GPU_MEMCPY_UNK;
  }
}


static void
convert_memcpy
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  ompt_record_target_data_op_t *d = &r->record.target_data_op;

  ga->kind = GPU_ACTIVITY_MEMCPY;

  ga->details.memcpy.correlation_id = d->host_op_id;
  *cid_ptr = d->host_op_id;

  ga->details.memcpy.bytes = d->bytes;
  ga->details.memcpy.copyKind = convert_memcpy_type(d->optype);
}


static void
convert_target_data_op
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  ompt_record_target_data_op_t *d = &r->record.target_data_op;

  switch(d->optype) {

  case ompt_target_data_transfer_to_device:
  case ompt_target_data_transfer_from_device:
    convert_memcpy(ga, r, cid_ptr);
    break;

  case ompt_target_data_alloc_async:
  case ompt_target_data_alloc:
    convert_alloc(ga, r, cid_ptr);
    break;

  case ompt_target_data_delete_async:
  case ompt_target_data_delete:
    convert_delete(ga, r, cid_ptr);
    break;

  case ompt_target_data_associate:
  case ompt_target_data_disassociate:
    convert_ptrop(ga, r, cid_ptr);
    break;

  default:
    convert_unknown(ga, r, cid_ptr);
    break;
  }

  gpu_interval_set(&ga->details.interval, r->time, d->end_time);
}


void
convert_target_submit
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  ompt_record_target_kernel_t *k = &r->record.target_kernel;

  ga->kind = GPU_ACTIVITY_KERNEL;
  ga->details.kernel.kernel_first_pc = ip_normalized_NULL;
  ga->details.kernel.correlation_id = k->host_op_id;
  *cid_ptr = k->host_op_id;

  gpu_interval_set(&ga->details.interval, r->time, k->end_time);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
ompt_activity_translate
(
 gpu_activity_t *ga,
 ompt_record_ompt_t *r,
 uint64_t *cid_ptr
)
{
  memset(ga, 0, sizeof(gpu_activity_t));
  switch (r->type) {

  case ompt_callback_target:
  case ompt_callback_target_emi:

    convert_target(ga,r, cid_ptr);
    break;

  case ompt_callback_target_data_op:
  case ompt_callback_target_data_op_emi:

    convert_target_data_op(ga,r, cid_ptr);
    break;

  case ompt_callback_target_submit:
  case ompt_callback_target_submit_emi:

    convert_target_submit(ga,r, cid_ptr);
    break;

  default:
    convert_unknown(ga, r, cid_ptr);
    break;
  }


  cstack_ptr_set(&(ga->next), 0);
}
