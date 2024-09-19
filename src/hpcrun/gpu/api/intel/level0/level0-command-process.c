// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#define _GNU_SOURCE

#include "level0-command-process.h"
#include "level0-data-node.h"
#include "level0-binary.h"
#include "level0-kernel-module-map.h"
#include "level0-api.h"

#include "../../../activity/gpu-activity-channel.h"
#include "../../../activity/gpu-activity-process.h"
#include "../../../activity/correlation/gpu-correlation-channel.h"
#include "../../../api/nvidia/cuda-correlation-id-map.h"
#include "../../../activity/correlation/gpu-host-correlation-map.h"
#include "../../../gpu-monitoring-thread-api.h"
#include "../../../gpu-application-thread-api.h"
#include "../../../activity/gpu-op-placeholders.h"
#include "../../../operation/gpu-operation-multiplexer.h"
#include "../../common/gpu-kernel-table.h"

#include "../../common/gpu-cct.h"

#ifdef ENABLE_GTPIN
#include "../gtpin/gtpin-instrumentation.h"
#endif

#include "../../../../audit/audit-api.h"
#include "../../../../safe-sampling.h"
#include "../../../../logical/common.h"
#include "../../../../utilities/hpcrun-nanotime.h"
#include "../../../../../common/lean/crypto-hash.h"
#include <stdatomic.h>
#include "../../../../../common/lean/usec_time.h"

#include "../../../../libmonitor/monitor.h"



#include "../../../common/gpu-print.h"

//*****************************************************************************
// local variables
//*****************************************************************************

// The thread itself how many pending operations
static __thread atomic_int level0_self_pending_operations = 0;

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
  PRINT("level0_kernel_translate: submit_time %llu, start %llu, end %llu\n", c->submit_time, start, end);
  ga->kind = GPU_ACTIVITY_KERNEL;
  ga->details.kernel.kernel_first_pc = ip_normalized_NULL;
  ga->details.kernel.correlation_id = (uint64_t)(c->event);
  ga->details.kernel.submit_time = c->submit_time;
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
  ga->details.memcpy.submit_time = c->submit_time;

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
  // Increment the operation counter for this thread
  atomic_fetch_add(&level0_self_pending_operations, 1);
  command_node->pending_operations = &level0_self_pending_operations;

  // Set up placeholder type
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_placeholder_type_t gpu_placeholder_node = gpu_placeholder_type_count;
  switch (command_node->type) {
    case LEVEL0_KERNEL: {
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_trace);
      gpu_placeholder_node = gpu_placeholder_type_kernel;
      break;
    }
    case LEVEL0_MEMCPY: {
      ze_memory_type_t src_type = command_node->details.memcpy.src_type;
      ze_memory_type_t dst_type = command_node->details.memcpy.dst_type;

      // use src and dst types to distinguish copyin and copyout
      if (src_type == ZE_MEMORY_TYPE_DEVICE && dst_type != ZE_MEMORY_TYPE_DEVICE) {
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyout);
        gpu_placeholder_node = gpu_placeholder_type_copyout;
      } else if (src_type != ZE_MEMORY_TYPE_DEVICE && dst_type == ZE_MEMORY_TYPE_DEVICE) {
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyin);
        gpu_placeholder_node = gpu_placeholder_type_copyin;
      } else {
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copy);
        gpu_placeholder_node = gpu_placeholder_type_copy;
      }
      break;
    }
    default:
      // FIXME: need to set gpu_placeholder_node to none, but there is not a none value
      break;
  }

  uint64_t correlation_id = (uint64_t)(command_node->event);
  cct_node_t *api_node =
    gpu_application_thread_correlation_callback(correlation_id);

  gpu_op_ccts_t gpu_op_ccts;
  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);

  if (command_node->type == LEVEL0_KERNEL) {
    ip_normalized_t kernel_ip;
    ze_kernel_handle_t kernel = command_node->details.kernel.kernel;
    size_t name_size = 0;
    f_zeKernelGetName(kernel, &name_size, NULL, command_node->dispatch);
    char* kernel_name = malloc(name_size);
    f_zeKernelGetName(kernel, &name_size, kernel_name, command_node->dispatch);
#ifdef ENABLE_GTPIN
    if (level0_gtpin_enabled()) {
      kernel_ip = gtpin_lookup_kernel_ip(kernel_name);
    } else
#endif  // ENABLE_GTPIN
    {
      kernel_ip = gpu_kernel_table_get(kernel_name, LOGICAL_MANGLING_CPP);
    }
    free(kernel_name);

    cct_node_t *kernel_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_kernel);
    command_node->kernel =
    gpu_cct_insert_always(kernel_ph, kernel_ip);

    cct_node_t *trace_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_trace);
    gpu_cct_insert(trace_ph, kernel_ip);
  }

  command_node->cct_node = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_node);

  hpcrun_safe_exit();

  gpu_application_thread_process_activities();

  // Generate host side operation timestamp
  command_node->submit_time = hpcrun_nanotime();

#ifdef ENABLE_GTPIN
  if (command_node->type == LEVEL0_KERNEL && level0_gtpin_enabled()) {
    // Callback to produce gtpin correlation
    gtpin_produce_runtime_callstack(&gpu_op_ccts);
  }
#endif
}

void
level0_command_end
(
  level0_data_node_t* command_node,
  uint64_t start,
  uint64_t end
)
{
  gpu_monitoring_thread_activities_ready();
  gpu_activity_t gpu_activity;
  gpu_activity_t* ga = &gpu_activity;
  memset(ga, 0, sizeof(gpu_activity_t));
  ga->cct_node = command_node->cct_node;
  PRINT("cct node %p, command node type %d\n", ga->cct_node, command_node->type);
  switch (command_node->type) {
    case LEVEL0_KERNEL:
      ga->cct_node = command_node->kernel;
      level0_kernel_translate(ga, command_node, start, end);
      break;
    case LEVEL0_MEMCPY:
      level0_memcpy_translate(ga, command_node, start, end);
      break;
    default:
      break;
  }

  cstack_ptr_set(&(ga->next), 0);

  // Send this activity record to the hpcrun monitoring thread
  gpu_operation_multiplexer_push(
    command_node->initiator_channel,
    command_node->pending_operations,
    ga
  );
}

void
level0_flush_and_wait
(
  void
)
{
  atomic_bool wait;
  atomic_store(&wait, true);
  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  gpu_activity.kind = GPU_ACTIVITY_FLUSH;
  gpu_activity.details.flush.wait = &wait;
  gpu_operation_multiplexer_push(gpu_activity_channel_get_local(), NULL, &gpu_activity);

  // Wait until operations are drained
  // Operation channel is FIFO
  while (atomic_load(&wait)) {}
}

void
level0_wait_for_self_pending_operations
(
  void
)
{
  while (atomic_load(&level0_self_pending_operations) != 0);
}
