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

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct_addr.h>
#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-function-id-map.h>
#include <hpcrun/gpu/gpu-host-correlation-map.h>


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
  ompt_record_target_t *t = &r->record.target;

  ga->kind = GPU_ACTIVITY_UNKNOWN;
  *cid_ptr = 0;

#if 0
  printf("\tTarget task: kind=%d endpoint=%d device=%d task_id=%lu target_id=%lu codeptr=%p\n",
	 target_rec.kind, target_rec.endpoint, target_rec.device_num,
	 target_rec.task_id, target_rec.target_id, 
#endif
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

# if 0
  TMSG(OMPT_ACTIVITY, "Memcpy copy CorrelationId %u", r->correlationId);
  TMSG(OMPT_ACTIVITY, "Memcpy copy kind %u", d->optype);
  TMSG(OMPT_ACTIVITY, "Memcpy copy bytes %lu", d->bytes);

  
  ga->details.memcpy.context_id = r->contextId;
  ga->details.memcpy.stream_id = r->streamId;
#endif

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

#if 0
  ( r->thread_id, r->target_id);


  printf("\tTarget data op: host_op_id=%lu optype=%d src_addr=%p "
	 "src_device=%d dest_addr=%p dest_device=%d bytes=%lu "
	 "end_time=%lu duration=%luus codeptr=%p\n",
	 d->host_op_id, d->optype,
	 d->src_addr, d->src_device_num,
	 d->dest_addr, d->dest_device_num,
#endif

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
  ga->details.kernel.correlation_id = k->host_op_id;
  *cid_ptr = k->host_op_id;

#if 0
  ( r->thread_id, r->target_id);
  printf("\tTarget kernel: host_op_id=%lu requested_num_teams=%u "
	 "granted_num_teams=%u end_time=%lu duration=%luus\n",
	 target_kernel_rec.host_op_id,
	 target_kernel_rec.requested_num_teams,
	 target_kernel_rec.granted_num_teams,

  ga->details.kernel.dynamicSharedMemory = r->dynamicSharedMemory;
  ga->details.kernel.staticSharedMemory = r->staticSharedMemory;
  ga->details.kernel.localMemoryTotal = r->localMemoryTotal;
  ga->details.kernel.device_id = r->deviceId;
  ga->details.kernel.context_id = r->contextId;
  ga->details.kernel.stream_id = r->streamId;
  ga->details.kernel.blocks = r->blockX * r->blockY * r->blockZ;


  uint32_t activeWarpsPerSM = 0;
  uint32_t maxActiveWarpsPerSM = 0;
  uint32_t threadRegisters = 0;
  uint32_t blockThreads = 0;
  uint32_t blockSharedMemory = 0;
  cupti_occupancy_analyze(r, &activeWarpsPerSM, &maxActiveWarpsPerSM,
			  &threadRegisters, &blockThreads, &blockSharedMemory);

  ga->details.kernel.activeWarpsPerSM = activeWarpsPerSM;
  ga->details.kernel.maxActiveWarpsPerSM = maxActiveWarpsPerSM;
  ga->details.kernel.threadRegisters = threadRegisters;
  ga->details.kernel.blockThreads = blockThreads;
  ga->details.kernel.blockSharedMemory = blockSharedMemory;
#endif

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
