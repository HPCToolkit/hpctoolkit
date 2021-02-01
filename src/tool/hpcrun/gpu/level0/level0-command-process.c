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

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-command-process.h"
#include "level0-data-node.h"

#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-host-correlation-map.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/safe-sampling.h>
#include <lib/prof-lean/usec_time.h>

//*****************************************************************************
// macro declarations
//*****************************************************************************
#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"


#define CPU_NANOTIME() (usec_time() * 1000)

//*****************************************************************************
// private operations
//*****************************************************************************

static void
level0_kernel_translate
(
  gpu_activity_t* ga,
  level0_data_node_t* c,
  uint64_t start,
  uint64_t end
)
{
  ga->kind = GPU_ACTIVITY_KERNEL;
  ga->details.kernel.correlation_id = (uint64_t)(c->event);
  gpu_interval_set(&ga->details.interval, start, end);
}

static void
level0_memcpy_translate
(
  gpu_activity_t* ga,
  level0_data_node_t* c,
  uint64_t start,
  uint64_t end
)
{
  PRINT("level0_memcpy_translate: src_type %d, dst_type %d, size %u\n",
    c->details.memcpy.src_type,
    c->details.memcpy.dst_type,
    c->details.memcpy.copy_size);

  ga->kind = GPU_ACTIVITY_MEMCPY;
  ga->details.memcpy.bytes = c->details.memcpy.copy_size;
  ga->details.memcpy.correlation_id = (uint64_t)(c->event);

  // Switch on memory src and dst types
  ga->details.memcpy.copyKind = GPU_MEMCPY_UNK;
  switch (c->details.memcpy.src_type) {
    case ZE_MEMORY_TYPE_HOST: {
      switch (c->details.memcpy.dst_type) {
        case ZE_MEMORY_TYPE_HOST:
          ga->details.memcpy.copyKind = GPU_MEMCPY_H2H;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          ga->details.memcpy.copyKind = GPU_MEMCPY_H2D;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          ga->details.memcpy.copyKind = GPU_MEMCPY_H2A;
          break;
        default:
          break;
      }
      break;
    }
    case ZE_MEMORY_TYPE_DEVICE: {
      switch (c->details.memcpy.dst_type) {
        case ZE_MEMORY_TYPE_HOST:
          ga->details.memcpy.copyKind = GPU_MEMCPY_D2H;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          ga->details.memcpy.copyKind = GPU_MEMCPY_D2D;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          ga->details.memcpy.copyKind = GPU_MEMCPY_D2A;
          break;
        default:
          break;
      }
      break;
    }
    case ZE_MEMORY_TYPE_SHARED: {
      switch (c->details.memcpy.dst_type) {
        case ZE_MEMORY_TYPE_HOST:
          ga->details.memcpy.copyKind = GPU_MEMCPY_A2H;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          ga->details.memcpy.copyKind = GPU_MEMCPY_A2D;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          ga->details.memcpy.copyKind = GPU_MEMCPY_A2A;
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
  gpu_interval_set(&ga->details.interval, start, end);
}

//*****************************************************************************
// interface operations
//*****************************************************************************


// Expand this function to create GPU side cct
void
level0_command_begin
(
  level0_data_node_t* command_node
)
{
  // Set up placeholder type
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  switch (command_node->type) {
    case LEVEL0_KERNEL: {
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_trace);
      break;
    }
    case LEVEL0_MEMCPY: {
      ze_memory_type_t src_type = command_node->details.memcpy.src_type;
      ze_memory_type_t dst_type = command_node->details.memcpy.dst_type;
// TODO: Do we need to distinguish copyin and copyout placeholder?
//       We already have host to host, host to device types to
//       distinguish copyin and copyout
//      if (src_type == ZE_MEMORY_TYPE_HOST && dst_type != ZE_MEMORY_TYPE_HOST) {
//        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyout);
//      } else if (src_type != ZE_MEMORY_TYPE_HOST && dst_type == ZE_MEMORY_TYPE_HOST) {
//        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyin);
//      } else {
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copy);
//      }
      break;
    }
    default:
      break;
  }

  uint64_t correlation_id = (uint64_t)(command_node->event);
  cct_node_t *api_node =
    gpu_application_thread_correlation_callback(correlation_id);

  gpu_op_ccts_t gpu_op_ccts;
  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  hpcrun_safe_exit();

  gpu_activity_channel_consume(gpu_metrics_attribute);

  // Generate notification entry
  uint64_t cpu_submit_time = CPU_NANOTIME();
  gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
}

void
level0_command_end
(
  level0_data_node_t* command_node,
  uint64_t start,
  uint64_t end
)
{
  uint64_t correlation_id = (uint64_t)(command_node->event);
  gpu_monitoring_thread_activities_ready();
  gpu_activity_t gpu_activity;
  gpu_activity_t* ga = &gpu_activity;
  memset(ga, 0, sizeof(gpu_activity_t));
  switch (command_node->type) {
    case LEVEL0_KERNEL:
      level0_kernel_translate(ga, command_node, start, end);
      break;
    case LEVEL0_MEMCPY:
      level0_memcpy_translate(ga, command_node, start, end);
      break;
    default:
      break;
  }

  cstack_ptr_set(&(ga->next), 0);
  if (gpu_correlation_id_map_lookup(correlation_id) == NULL) {
    gpu_correlation_id_map_insert(correlation_id, correlation_id);
  }
  gpu_activity_process(&gpu_activity);
  // gpu_activity_process will delete items in gpu-correlation-id-map
  // We will need to delete items in gpu-host-correlation-map
  gpu_host_correlation_map_delete(correlation_id);
}
