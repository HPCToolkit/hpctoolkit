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
#include <stdlib.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <sys/stat.h>  // mkdir
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <gtpin.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/cct/cct.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/files.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-correlation.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-operation-multiplexer.h>
#include <hpcrun/gpu/gpu-host-correlation-map.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>

#include <lib/prof-lean/crypto-hash.h>
#include <lib/prof-lean/spinlock.h>

#include "gtpin-correlation-id-map.h"
#include "gtpin-instrumentation.h"
#include "kernel-data.h"
#include "kernel-data-map.h"

//******************************************************************************
// local data
//******************************************************************************

#define MAX_STR_SIZE 1024
#define KERNEL_SUFFIX ".kernel"

// TODO(Aaron): Why there are so many correlation ids
static atomic_ullong correlation_id;

static spinlock_t files_lock = SPINLOCK_UNLOCKED;

static bool gtpin_use_runtime_callstack = false;

static __thread uint64_t gtpin_correlation_id = 0;
static __thread uint64_t gtpin_cpu_submit_time = 0;
static __thread gpu_op_ccts_t gtpin_gpu_op_ccts;
static __thread bool gtpin_first = true;

//******************************************************************************
// private operations
//******************************************************************************

static void
knobAddBool
(
 const char *name,
 bool value
)
{
  GTPinKnob knob = KNOB_FindArg(name);
  assert(knob != NULL);
  KnobValue knob_value;
  knob_value.value._bool = value;
  knob_value.type = KNOB_TYPE_BOOL;
  KNOB_STATUS status = KNOB_AddValue(knob, &knob_value);
  assert(status == KNOB_STATUS_SUCCESS);
}


void
initializeInstrumentation
(
 void
)
{
  atomic_store(&correlation_id, 100000000);  // to avoid conflict with opencl operation correlation ids, we start instrumentation ids with 5000 (TODO(Aaron):FIX)
}


static uint64_t
getCorrelationId
(
 void
)
{
  return atomic_fetch_add(&correlation_id, 1);
}


static void
createKernelNode
(
 uint64_t correlation_id
)
{
  uint64_t cpu_submit_time = hpcrun_nanotime();

  if (gtpin_use_runtime_callstack) {
    // XXX(Keren): gtpin's call stack is a mass, better to use opencl's call path
    // onKernelRun->clEnqueueNDRangeKernel_wrapper->opencl_subscriber_callback
    if (gtpin_first) {
      // gtpin callback->runtime callback
      gtpin_correlation_id = correlation_id;
      gtpin_cpu_submit_time = cpu_submit_time;
    } else {
      // runtime callback->gtpin callback
      gpu_activity_channel_t *activity_channel = gpu_activity_channel_get();
      gtpin_correlation_id_map_insert(correlation_id, &gtpin_gpu_op_ccts, activity_channel, cpu_submit_time);
    }
  } else {
    cct_node_t *api_node = gpu_application_thread_correlation_callback(correlation_id);

    gpu_op_ccts_t gpu_op_ccts;
    gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);

    hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
    hpcrun_safe_exit();

    gpu_activity_channel_t *activity_channel = gpu_activity_channel_get();
    gtpin_correlation_id_map_insert(correlation_id, &gpu_op_ccts, activity_channel, cpu_submit_time);
  }
}


static bool
writeBinary
(
 const char *file_name,
 const void *binary,
 size_t binary_size
)
{
  int fd;
  errno = 0;
  fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (errno == EEXIST) {
    close(fd);
    return true;
  }
  if (fd >= 0) {
    // Success
    if (write(fd, binary, binary_size) != binary_size) {
      close(fd);
      return false;
    } else {
      close(fd);
      return true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    return false;
  }
}


void
computeBinaryHash
(
 const char *binary,
 size_t binary_size,
 char *file_name
)
{
  // Compute hash for the binary
  unsigned char hash[HASH_LENGTH];
  crypto_hash_compute((const unsigned char *)binary, binary_size, hash, HASH_LENGTH);

  size_t i;
  size_t used = 0;
  used += sprintf(&file_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&file_name[used], "%s", "/intel/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  for (i = 0; i < HASH_LENGTH; ++i) {
    used += sprintf(&file_name[used], "%02x", hash[i]);
  }
  used += sprintf(&file_name[used], "%s", ".gpubin");
}


static uint32_t
findOrAddKernelModule
(
 GTPinKernel kernel
)
{
  uint32_t kernel_elf_size = 0;
  GTPINTOOL_STATUS status = GTPin_GetElf(kernel, 0, NULL, &kernel_elf_size);
  assert(status == GTPINTOOL_STATUS_SUCCESS);

  char *kernel_elf = (char *)malloc(sizeof(char) * kernel_elf_size);
  status = GTPin_GetElf(kernel, kernel_elf_size, kernel_elf, NULL);
  assert(status == GTPINTOOL_STATUS_SUCCESS);

  // Create file name
  char file_name[PATH_MAX];
  memset(file_name, 0, PATH_MAX);
  computeBinaryHash(kernel_elf, kernel_elf_size, file_name);

  // Write a file if does not exist
  spinlock_lock(&files_lock);
  writeBinary(file_name, kernel_elf, kernel_elf_size);
  spinlock_unlock(&files_lock);

  free(kernel_elf);

  strncat(file_name, KERNEL_SUFFIX, strlen(KERNEL_SUFFIX));

  uint32_t module_id = 0;

  hpcrun_loadmap_lock();
  load_module_t *module = hpcrun_loadmap_findByName(file_name);
  if (module == NULL) {
    module_id = hpcrun_loadModule_add(file_name);
  } else {
    // Find module
    module_id = module->id;
  }
  hpcrun_loadmap_unlock();

  return module_id;
}


static void
activityNotify
(
 void
)
{
  gpu_monitoring_thread_activities_ready();
}


static void
kernelBlockActivityTranslate
(
 gpu_activity_t *ga,
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t execution_count
)
{
  memset(&ga->details.kernel_block, 0, sizeof(gpu_kernel_block_t));
  ga->details.kernel_block.external_id = correlation_id;
  ga->details.kernel_block.pc.lm_id = (uint16_t)loadmap_module_id;
  ga->details.kernel_block.pc.lm_ip = (uintptr_t)offset;
  ga->details.kernel_block.execution_count = execution_count;
  ga->kind = GPU_ACTIVITY_KERNEL_BLOCK;

  cstack_ptr_set(&(ga->next), 0);
}


static void
kernelBlockActivityProcess
(
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t execution_count,
 gpu_activity_channel_t *activity_channel,
 cct_node_t *host_op_node
)
{
  gpu_activity_t ga;
  kernelBlockActivityTranslate(&ga, correlation_id, loadmap_module_id, offset, execution_count);

  ip_normalized_t ip = ga.details.kernel_block.pc;
  cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip); // how to set the ip_norm
  if (cct_child) {
    ga.cct_node = cct_child;
    gpu_operation_multiplexer_push(activity_channel, NULL, &ga);
  }
}


static void
onKernelBuild
(
 GTPinKernel kernel,
 void *v
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;

  assert(kernel_data_map_lookup((uint64_t)kernel) == 0);

  kernel_data_t kernel_data;
  kernel_data.loadmap_module_id = findOrAddKernelModule(kernel);
  kernel_data.kind = KERNEL_DATA_GTPIN;

  kernel_data_gtpin_block_t *gtpin_block_head = NULL;
  kernel_data_gtpin_block_t *gtpin_block_curr = NULL;

  for (GTPinBBL block = GTPin_BBLHead(kernel); GTPin_BBLValid(block); block = GTPin_BBLNext(block)) {
    GTPinINS head = GTPin_InsHead(block);
    GTPinINS tail = GTPin_InsTail(block);
    assert(GTPin_InsValid(head));
    int32_t head_offset = GTPin_InsOffset(head);
    int32_t tail_offset = GTPin_InsOffset(tail);

    GTPinMem mem = NULL;
    status = GTPin_MemClaim(kernel, sizeof(uint32_t), &mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);
    status = GTPin_OpcodeprofInstrument(head, mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

    kernel_data_gtpin_block_t *gtpin_block = (kernel_data_gtpin_block_t *)hpcrun_malloc(sizeof(kernel_data_gtpin_block_t));
    gtpin_block->head_offset = head_offset;
    gtpin_block->tail_offset = tail_offset;
    gtpin_block->mem = mem;
    gtpin_block->next = NULL;

    if (gtpin_block_head == NULL) {
      gtpin_block_head = gtpin_block;
    } else {
      gtpin_block_curr->next = gtpin_block;
    }
    gtpin_block_curr = gtpin_block;
    
    // while loop that iterates for each instruction in the block and adds an offset entry in map
    int32_t offset = head_offset;
    GTPinINS inst = GTPin_InsHead(block);
    kernel_data_gtpin_inst_t *gtpin_inst_curr = NULL;
    while (offset <= tail_offset && offset != -1) {
      kernel_data_gtpin_inst_t *gtpin_inst = (kernel_data_gtpin_inst_t *)hpcrun_malloc(sizeof(kernel_data_gtpin_inst_t));
      gtpin_inst->offset = offset;
      if (gtpin_inst_curr == NULL) {
        gtpin_block_curr->inst = gtpin_inst;
      } else {
        gtpin_inst_curr->next = gtpin_inst;
      }
      gtpin_inst_curr = gtpin_inst;
      inst = GTPin_InsNext(inst);
      offset = GTPin_InsOffset(inst);
    }
  }

  if (gtpin_block_head != NULL) {
    kernel_data_gtpin_t *kernel_data_gtpin = (kernel_data_gtpin_t *)hpcrun_malloc(sizeof(kernel_data_gtpin_t));
    kernel_data_gtpin->kernel_id = (uint64_t)kernel;
    kernel_data_gtpin->block = gtpin_block_head;
    kernel_data.data = kernel_data_gtpin; 
    kernel_data_map_insert((uint64_t)kernel, kernel_data);
  }

  // add these details to cct_node. If thats not needed, we can create the kernel_cct in onKernelComplete
  ETMSG(OPENCL, "onKernelBuild complete. Inserted key: %"PRIu64 "",(uint64_t)kernel);
}


static void
onKernelRun
(
 GTPinKernelExec kernelExec,
 void *v
)
{
  ETMSG(OPENCL, "onKernelRun starting. Inserted: correlation %"PRIu64"", (uint64_t)kernelExec);

  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPin_KernelProfilingActive(kernelExec, 1);
  assert(status == GTPINTOOL_STATUS_SUCCESS);

  createKernelNode((uint64_t)kernelExec);
}


static void
onKernelComplete
(
 GTPinKernelExec kernelExec,
 void *v
)
{
  // Receive correlations from the host thread.
  // XXX(Keren): This is done usually at the monitor thread, but not guaranteed.
  // For safety concern, we need to adopt the multiplexer framework.
  //activityNotify();  
  
  uint64_t correlation_id = (uint64_t)kernelExec;

  gtpin_correlation_id_map_entry_t *entry =
    gtpin_correlation_id_map_lookup(correlation_id);

  ETMSG(OPENCL, "onKernelComplete starting. Lookup: correlation %"PRIu64", result %p", correlation_id, entry);

  if (entry == NULL) {
    // XXX(Keren): the opencl/level zero api's kernel launch is not wrapped
    return;
  }

  gpu_activity_channel_t *activity_channel = gtpin_correlation_id_map_entry_activity_channel_get(entry);
  gpu_op_ccts_t gpu_op_ccts = gtpin_correlation_id_map_entry_op_ccts_get(entry);
  cct_node_t *host_op_node = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_kernel);

  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPinKernel kernel = GTPin_KernelExec_GetKernel(kernelExec);
  ETMSG(OPENCL, "onKernelComplete starting. Lookup: kernel: %"PRIu64"", (uint64_t)kernel);
  assert(kernel_data_map_lookup((uint64_t)kernel) != 0);

  kernel_data_map_entry_t *kernel_data_map_entry = kernel_data_map_lookup((uint64_t)kernel);
  assert(kernel_data_map_entry != NULL);

  kernel_data_t kernel_data = kernel_data_map_entry_kernel_data_get(kernel_data_map_entry);
  assert(kernel_data.kind == KERNEL_DATA_GTPIN);

  kernel_data_gtpin_t *kernel_data_gtpin = (kernel_data_gtpin_t *)kernel_data.data; 
  kernel_data_gtpin_block_t *block = kernel_data_gtpin->block;

  while (block != NULL) {
    uint32_t thread_count = GTPin_MemSampleLength(block->mem);
    assert(thread_count > 0);

    uint32_t total = 0, value = 0;
    for (uint32_t tid = 0; tid < thread_count; ++tid) {
      status = GTPin_MemRead(block->mem, tid, sizeof(uint32_t), (char*)(&value), NULL);
      assert(status == GTPINTOOL_STATUS_SUCCESS);
      total += value;
    }
    uint64_t execution_count = total; // + bm->val 

    kernel_data_gtpin_inst_t *inst = block->inst;
    while (inst != NULL) {
      kernelBlockActivityProcess(correlation_id, kernel_data.loadmap_module_id,
        inst->offset, execution_count, activity_channel, host_op_node);
      inst = inst->next;
    }
    block = block->next;
    //how to make offset the primary key within the cct and += the execution value for existing ccts?
  }
}

//******************************************************************************
// interface operations
//******************************************************************************

void
gtpin_enable_profiling
(
 void
)
{
  ETMSG(OPENCL, "inside enableProfiling");
  initializeInstrumentation();
  knobAddBool("silent_warnings", true);

#if 0
  if (utils::GetEnv("PTI_GEN12") != nullptr) {
    KnobAddBool("gen12_1", true);
  }
#endif

  // Use opencl/level zero runtime stack
  gtpin_use_runtime_callstack = true;

  GTPin_OnKernelBuild(onKernelBuild, NULL);
  GTPin_OnKernelRun(onKernelRun, NULL);
  GTPin_OnKernelComplete(onKernelComplete, NULL);

  GTPIN_Start();
}


void
gtpin_produce_runtime_callstack
(
 gpu_op_ccts_t *gpu_op_ccts
)
{
  if (gtpin_use_runtime_callstack) {
    if (gtpin_correlation_id != 0) {
      // gtpin callback->opencl callback
      gpu_activity_channel_t *activity_channel = gpu_activity_channel_get();
      gtpin_correlation_id_map_insert(gtpin_correlation_id, gpu_op_ccts, activity_channel, gtpin_cpu_submit_time);
      gtpin_correlation_id = 0;
      gtpin_first = true;
    } else {
      // opencl callback->gtpin callback;
      gtpin_gpu_op_ccts = *gpu_op_ccts;      
      gtpin_first = false;
    }
  }
}
