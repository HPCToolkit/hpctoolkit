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
// system includes
//******************************************************************************

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-trace-item.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-context-id-map.h>
#include <hpcrun/gpu/gpu-event-id-map.h>
#include <hpcrun/gpu/gpu-function-id-map.h>
#include <hpcrun/gpu/gpu-host-correlation-map.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/trace.h>



//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// private operations
//******************************************************************************


static void
gpu_context_stream_trace
(
 uint32_t device_id,
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_item_t *ti
)
{
  if (hpcrun_trace_isactive()) {
    gpu_context_id_map_stream_process(device_id, context_id, stream_id, gpu_trace_produce, ti);
  }
}


static void
gpu_context_trace
(
 uint32_t context_id,
 gpu_trace_item_t *ti
)
{
  if (hpcrun_trace_isactive()) {
    gpu_context_id_map_context_process(context_id, gpu_trace_produce, ti);
  }
}


static void
trace_item_set
(
 gpu_trace_item_t *ti,
 gpu_activity_t *ga,
 gpu_host_correlation_map_entry_t *host_op_entry,
 cct_node_t *call_path_leaf
)
{
  uint64_t cpu_submit_time =
    gpu_host_correlation_map_entry_cpu_submit_time(host_op_entry);

  gpu_trace_item_produce(ti, cpu_submit_time, ga->details.interval.start,
			 ga->details.interval.end, call_path_leaf);
}


static void
attribute_activity
(
 gpu_host_correlation_map_entry_t *hc,
 gpu_activity_t *activity,
 cct_node_t *cct_node
)
{
  gpu_activity_channel_t *channel =
    gpu_host_correlation_map_entry_channel_get(hc);
  activity->cct_node = cct_node;
  gpu_activity_channel_produce(channel, activity);
}


static void
gpu_memcpy_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.memcpy.correlation_id;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      gpu_placeholder_type_t mct;
      switch (activity->details.memcpy.copyKind) {
        case GPU_MEMCPY_H2D:
          mct = gpu_placeholder_type_copyin;
          break;
        case GPU_MEMCPY_D2H:
          mct = gpu_placeholder_type_copyout;
          break;
        default:
          mct = gpu_placeholder_type_copy;
          break;
      }
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_op_cct_get(host_op_entry, mct);
      if (host_op_node == NULL) {
        // If we cannot find a perfect match for the operation
        // e.g. cuMemcpy
        host_op_node = gpu_host_correlation_map_entry_op_cct_get(host_op_entry,
          gpu_placeholder_type_copy);
      }

      assert(host_op_node != NULL);
      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_entry, host_op_node);

      gpu_context_stream_trace
        (activity->details.memcpy.device_id, activity->details.memcpy.context_id,
         activity->details.memcpy.stream_id, &entry_trace);

      attribute_activity(host_op_entry, activity, host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(correlation_id);
  } else {
    PRINT("Memcpy copy correlation_id %u cannot be found\n", correlation_id);
  }
  PRINT("Memcpy copy CorrelationId %u\n", correlation_id);
  PRINT("Memcpy copy kind %u\n", activity->details.memcpy.copyKind);
  PRINT("Memcpy copy bytes %lu\n", activity->details.memcpy.bytes);
}


static void
gpu_unknown_process
(
 gpu_activity_t *activity
)
{
  PRINT("Unknown activity kind %d\n", activity->kind);
}

static void
gpu_sample_process
(
 gpu_activity_t* sample
)
{
  uint32_t correlation_id = sample->details.pc_sampling.correlation_id;

  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);

  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);

    ip_normalized_t ip = sample->details.pc_sampling.pc;

    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);

    if (host_op_entry != NULL) {
      PRINT("external_id %lu\n", external_id);

      bool more_samples =
        gpu_host_correlation_map_samples_increase
        (external_id, sample->details.pc_sampling.samples);

      if (!more_samples) {
        gpu_correlation_id_map_delete(correlation_id);
      }

      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_op_function_get(host_op_entry);

      cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip);
      if (cct_child) {
        PRINT("cct_child %p\n", cct_child);
        attribute_activity(host_op_entry, sample, cct_child);
      }
    } else {
      PRINT("host_map_entry %lu not found\n", external_id);
    }
  } else {
    PRINT("correlation_id_map_entry %u not found\n", correlation_id);
  }
}


static void
gpu_sampling_info_process
(
 gpu_activity_t *sri
)
{
  uint32_t correlation_id = sri->details.pc_sampling_info.correlation_id;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_op_function_get(host_op_entry);

      attribute_activity(host_op_entry, sri, host_op_node);
    }
    // sample info is the last record for a given correlation id
    bool more_samples =
      gpu_host_correlation_map_total_samples_update
      (external_id, sri->details.pc_sampling_info.totalSamples -
       sri->details.pc_sampling_info.droppedSamples);
    if (!more_samples) {
      gpu_correlation_id_map_delete(correlation_id);
    }
  }
  hpcrun_stats_acc_samples_add(sri->details.pc_sampling_info.totalSamples);
  hpcrun_stats_acc_samples_dropped_add(sri->details.pc_sampling_info.droppedSamples);
}


static void
gpu_correlation_process
(
 gpu_activity_t *activity
)
{
  uint32_t gpu_correlation_id = activity->details.correlation.correlation_id;
  uint64_t host_correlation_id = activity->details.correlation.host_correlation_id;

  if (gpu_correlation_id_map_lookup(gpu_correlation_id) == NULL) {
    gpu_correlation_id_map_insert(gpu_correlation_id, host_correlation_id);
  } else {
    gpu_correlation_id_map_external_id_replace(gpu_correlation_id, host_correlation_id);
  }
  PRINT("Correlation: native_correlation %u --> host_correlation %lu\n", 
      gpu_correlation_id, host_correlation_id);
}


static void
gpu_memset_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.memset.correlation_id;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id = gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
	gpu_host_correlation_map_entry_op_cct_get(host_op_entry,
						  gpu_placeholder_type_memset);

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_entry, host_op_node);

      gpu_context_stream_trace
        (activity->details.memset.device_id, activity->details.memset.context_id,
         activity->details.memset.stream_id, &entry_trace);

      attribute_activity(host_op_entry, activity, host_op_node);

      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(correlation_id);
  }
  PRINT("Memset CorrelationId %u\n", correlation_id);
  PRINT("Memset kind %u\n", activity->details.memset.memKind);
  PRINT("Memset bytes %lu\n", activity->details.memset.bytes);
}


static void
gpu_function_process
(
 gpu_activity_t *activity
)
{
  gpu_function_id_map_insert(activity->details.function.function_id, activity->details.function.pc);
  PRINT("Function id %u\n", activity->details.function.function_id);
}


static void
gpu_kernel_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.kernel.correlation_id;

  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    gpu_correlation_id_map_kernel_update
      (correlation_id, activity->details.kernel.device_id,
       activity->details.interval.start, activity->details.interval.end);

    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);

    if (host_op_entry != NULL) {
      cct_node_t *func_ph =
        gpu_host_correlation_map_entry_op_cct_get(host_op_entry,
						  gpu_placeholder_type_trace);

      cct_node_t *func_node = hpcrun_cct_children(func_ph); // only child

      if (func_node == NULL) {
	// in case placeholder doesn't have a child
        func_node = func_ph;
      }

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_entry, func_node);

      gpu_context_stream_trace
        (activity->details.kernel.device_id, activity->details.kernel.context_id,
         activity->details.kernel.stream_id, &entry_trace);

      cct_node_t *kernel_ph =
        gpu_host_correlation_map_entry_op_cct_get(host_op_entry,
						  gpu_placeholder_type_kernel);

      cct_node_t *kernel_node;
      if (func_node == func_ph) {
	// in case placeholder doesn't have a child
        kernel_node = kernel_ph;
      } else {
	// find the proper child of kernel_ph
	cct_addr_t *addr = hpcrun_cct_addr(func_node);
	kernel_node = hpcrun_cct_insert_ip_norm(kernel_ph, addr->ip_norm);
      }

      attribute_activity(host_op_entry, activity, kernel_node);
    }
  } else {
    PRINT("Kernel execution correlation_id %u cannot be found\n", correlation_id);
  }

  PRINT("Kernel execution deviceId %u\n", activity->details.kernel.device_id);
  PRINT("Kernel execution CorrelationId %u\n", correlation_id);
}


static void
gpu_kernel_block_process
(
 gpu_activity_t* activity
)
{
  uint64_t external_id = activity->details.kernel_block.external_id;
  ip_normalized_t ip = activity->details.kernel_block.pc;

  gpu_host_correlation_map_entry_t *host_op_entry =
    gpu_host_correlation_map_lookup(external_id);

  if (host_op_entry != NULL) {
    PRINT("external_id %lu\n", external_id);

    cct_node_t *host_op_node =
      gpu_host_correlation_map_entry_op_function_get(host_op_entry);

    // create a child cct node that contains 2 metrics: offset of block head wrt. original binary, dynamic execution count of block
    cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip); // how to set the ip_norm
    if (cct_child) {
      PRINT("cct_child %p\n", cct_child);
      attribute_activity(host_op_entry, activity, cct_child);
    }
  } else {
    PRINT("host_map_entry %lu not found\n", external_id);
  }
}


static void
gpu_synchronization_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.synchronization.correlation_id;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_op_cct_get(host_op_entry,
          gpu_placeholder_type_sync);

      attribute_activity(host_op_entry, activity, host_op_node);

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_entry, host_op_node);

      if (activity->kind == GPU_ACTIVITY_SYNCHRONIZATION) {
        uint32_t context_id = activity->details.synchronization.context_id;
        uint32_t stream_id = activity->details.synchronization.stream_id;
        uint32_t event_id = activity->details.synchronization.event_id;

        switch (activity->details.synchronization.syncKind) {
          case GPU_SYNC_STREAM:
          case GPU_SYNC_STREAM_EVENT_WAIT:
            // Insert a event for a specific stream
            PRINT("Add context %u stream %u sync\n", context_id, stream_id);
            gpu_context_stream_trace(IDTUPLE_INVALID, context_id, stream_id, &entry_trace);
            break;
          case GPU_SYNC_CONTEXT:
            // Insert events for all current active streams
            // TODO(Keren): What if the stream is created
            PRINT("Add context %u sync\n", context_id);
            gpu_context_trace(context_id, &entry_trace);
            break;
          case GPU_SYNC_EVENT:
            {
              // Find the corresponding stream that records the event
              gpu_event_id_map_entry_t *event_id_entry = gpu_event_id_map_lookup(event_id);
              if (event_id_entry != NULL) {
                context_id = gpu_event_id_map_entry_context_id_get(event_id_entry);
                stream_id = gpu_event_id_map_entry_stream_id_get(event_id_entry);
                PRINT("Add context %u stream %u event %u sync\n", context_id, stream_id, event_id);
                gpu_context_stream_trace(IDTUPLE_INVALID, context_id, stream_id, &entry_trace);
              }
              break;
            }
          default:
            // invalid
            PRINT("Invalid synchronization %u\n", correlation_id);
        }
      }
      // TODO(Keren): handle event synchronization
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(correlation_id);
  }
  PRINT("Synchronization CorrelationId %u\n", correlation_id);
}


static void
gpu_cdpkernel_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.cdpkernel.correlation_id;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *func_ph =
        gpu_host_correlation_map_entry_op_function_get(host_op_entry);

      cct_node_t *func_node = hpcrun_leftmost_child(func_ph);
      if (func_node == NULL) {
	// in case placeholder doesn't have a child
        func_node = func_ph;
      }

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_entry, func_node);

      gpu_context_stream_trace
        (activity->details.cdpkernel.device_id, activity->details.cdpkernel.context_id,
         activity->details.cdpkernel.stream_id, &entry_trace);
    }
    gpu_correlation_id_map_delete(correlation_id);
  }
  PRINT("Cdp Kernel CorrelationId %u\n", correlation_id);
}


static void
gpu_event_process
(
 gpu_activity_t *activity
)
{
  uint32_t event_id = activity->details.event.event_id;
  uint32_t context_id = activity->details.event.context_id;
  uint32_t stream_id = activity->details.event.stream_id;
  gpu_event_id_map_insert(event_id, context_id, stream_id);

  PRINT("GPU event %u\n", event_id);
}


static void
gpu_memory_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.memory.correlation_id;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      gpu_placeholder_type_t ph = gpu_placeholder_type_alloc;
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_op_cct_get(host_op_entry, ph);
      assert(host_op_node != NULL);
      // Memory allocation does not always happen on the device
      // Do not send it to trace channels
      attribute_activity(host_op_entry, activity, host_op_node);
    }
    gpu_correlation_id_map_delete(correlation_id);
  } else {
    PRINT("Memory correlation_id %u cannot be found\n", correlation_id);
  }
  PRINT("Memory CorrelationId %u\n", correlation_id);
  PRINT("Memory kind %u\n", activity->details.memory.memKind);
  PRINT("Memory bytes %lu\n", activity->details.memory.bytes);
}


static void
gpu_instruction_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->details.instruction.correlation_id;
  ip_normalized_t pc = activity->details.instruction.pc;
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    PRINT("external_id %lu\n", external_id);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *func_ph =
        gpu_host_correlation_map_entry_op_function_get(host_op_entry);

      cct_node_t *func_ins = hpcrun_cct_insert_ip_norm(func_ph, pc);
      attribute_activity(host_op_entry, activity, func_ins);
    }
  }
  PRINT("Instruction correlation_id %u\n", correlation_id);
}


//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_activity_process
(
 gpu_activity_t *ga
)
{
  switch (ga->kind) {

  case GPU_ACTIVITY_PC_SAMPLING:
    gpu_sample_process(ga);
    break;

  case GPU_ACTIVITY_PC_SAMPLING_INFO:
    gpu_sampling_info_process(ga);
    break;

  case GPU_ACTIVITY_EXTERNAL_CORRELATION:
    gpu_correlation_process(ga);
    break;

  case GPU_ACTIVITY_MEMCPY:
    gpu_memcpy_process(ga);
    break;

  case GPU_ACTIVITY_MEMSET:
    gpu_memset_process(ga);
    break;

  case GPU_ACTIVITY_FUNCTION:
    gpu_function_process(ga);
    break;

  case GPU_ACTIVITY_KERNEL:
    gpu_kernel_process(ga);
    break;

  case GPU_ACTIVITY_KERNEL_BLOCK:
    gpu_kernel_block_process(ga);
    break;

  case GPU_ACTIVITY_SYNCHRONIZATION:
    gpu_synchronization_process(ga);
    break;

  case GPU_ACTIVITY_MEMORY:
    gpu_memory_process(ga);
    break;

  case GPU_ACTIVITY_LOCAL_ACCESS:
  case GPU_ACTIVITY_GLOBAL_ACCESS:
  case GPU_ACTIVITY_BRANCH:
    gpu_instruction_process(ga);
    break;

  case GPU_ACTIVITY_CDP_KERNEL:
    gpu_cdpkernel_process(ga);
    break;

  case GPU_ACTIVITY_EVENT:
    gpu_event_process(ga);
    break;

  case GPU_ACTIVITY_MEMCPY2:
  default:
    gpu_unknown_process(ga);
    break;
  }
}

