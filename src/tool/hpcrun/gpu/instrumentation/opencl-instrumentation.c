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
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/opencl/opencl-api.h>
#include <hpcrun/gpu/opencl/opencl-intercept.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>

#include <lib/prof-lean/crypto-hash.h>
#include <lib/prof-lean/spinlock.h>

#include "opencl-instrumentation.h"



//******************************************************************************
// local data
//******************************************************************************

#define MAX_STR_SIZE 1024

// TODO(Aaron): Why there are so many correlation ids
static atomic_long correlation_id;

static spinlock_t files_lock = SPINLOCK_UNLOCKED;

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
  atomic_store(&correlation_id, 5000);  // to avoid conflict with opencl operation correlation ids, we start instrumentation ids with 5000 (TODO(Aaron):FIX)
}


static uint32_t
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
  cct_node_t *api_node = gpu_application_thread_correlation_callback(correlation_id);
  gpu_correlation_id_map_insert(correlation_id, correlation_id);

  gpu_op_ccts_t gpu_op_ccts;
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);

  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  hpcrun_safe_exit();

  gpu_activity_channel_consume(gpu_metrics_attribute);
  uint64_t cpu_submit_time = hpcrun_nanotime();
  gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
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
  crypto_hash_compute(binary, binary_size, hash, HASH_LENGTH);

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
  char kernel_name[MAX_STR_SIZE];
  GTPINTOOL_STATUS status;

  status = GTPin_KernelGetName(kernel, MAX_STR_SIZE, kernel_name, NULL);
  assert(status == GTPINTOOL_STATUS_SUCCESS);

  uint32_t kernel_elf_size = 0;
  status = GTPin_GetElf(kernel, 0, NULL, &kernel_elf_size);
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

  strncat(file_name, ".", 1);
  strncat(file_name, kernel_name, strlen(kernel_name));

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
 uint32_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t execution_count
)
{
  memset(&ga->details.kernel_block, 0, sizeof(gpu_kernel_block_t));
  ga->details.kernel_block.correlation_id = correlation_id;
  ga->details.kernel_block.pc.lm_id = (uint16_t)loadmap_module_id;
  ga->details.kernel_block.pc.lm_ip = (uintptr_t)offset;
  ga->details.kernel_block.offset = offset;
  ga->details.kernel_block.execution_count = execution_count;
  ga->kind = GPU_ACTIVITY_KERNEL_BLOCK;

  cstack_ptr_set(&(ga->next), 0);
}


static void
kernelBlockActivityProcess
(
 gpu_activity_t *ga,
 uint32_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t execution_count
)
{
  kernelBlockActivityTranslate(ga, correlation_id, loadmap_module_id, offset, execution_count);
  gpu_activity_process(ga);
}


static void
onKernelBuild
(
 GTPinKernel kernel,
 void *v
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;

  assert(kernel_memory_map_lookup1((uint64_t)kernel) == 0);
  assert(kernel_data_map_lookup1((uint64_t)kernel) == 0);

  KernelData data;

  uint32_t correlation_id = getCorrelationId();
  data.kernel_cct_correlation_id = correlation_id;
  createKernelNode(correlation_id);

  data.call_count = 0;
  data.loadmap_module_id = findOrAddKernelModule(kernel);

  kernel_data_map_insert1((uint64_t)kernel, data);

  mem_pair_node *h;
  mem_pair_node *current;
  bool isHeadNull = true;

  for (GTPinBBL block = GTPin_BBLHead(kernel); GTPin_BBLValid(block); block = GTPin_BBLNext(block)) {
    GTPinINS head = GTPin_InsHead(block);
    assert(GTPin_InsValid(head));

    int32_t offset = GTPin_InsOffset(head);

    GTPinMem mem = NULL;
    status = GTPin_MemClaim(kernel, sizeof(uint32_t), &mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

    status = GTPin_OpcodeprofInstrument(head, mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

    // TODO(Aaron): when using hpcrun_malloc, find a way to recycle memory
    mem_pair_node *m = hpcrun_malloc(sizeof(mem_pair_node));
    m->offset = offset;
    m->mem = mem;
    m->next = NULL;

    if (isHeadNull == true) {
      h = m;
      current = m;
      isHeadNull = false;
    } else {
      current->next = m;
      current = current->next;
    }
  }
  if (h != NULL) {
    // TODO(Aaron): naming insert1/insert2 is confusing
    kernel_memory_map_insert1((uint64_t)kernel, h);
  }

  gpu_activity_channel_consume(gpu_metrics_attribute);
  // 
  // m->next = NULL;
  // add these details to cct_node. If thats not needed, we can create the kernel_cct in onKernelComplete
  // XXX(Aaron): what is this for?
  //data.name = kernel_name;
  ETMSG(OPENCL, "onKernelBuild complete. Inserted key: %"PRIu64 "",(uint64_t)kernel);
}


static void
onKernelRun
(
 GTPinKernelExec kernelExec,
 void *v
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPin_KernelProfilingActive(kernelExec, 1);
  assert(status == GTPINTOOL_STATUS_SUCCESS);
}


static void
onKernelComplete
(
 GTPinKernelExec kernelExec,
 void *v
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPinKernel kernel = GTPin_KernelExec_GetKernel(kernelExec);
  ETMSG(OPENCL, "onKernelComplete starting. Lookup: key: %"PRIu64 "",(uint64_t)kernel);
  assert(kernel_data_map_lookup1((uint64_t)kernel) != 0);
  assert(kernel_memory_map_lookup1((uint64_t)kernel) != 0);

  // TODO(Aaron): rename lookup methods, do not use magic numbers
  kernel_data_map_t *kernel_data_list = kernel_data_map_lookup1((uint64_t)kernel);
  KernelData data = kernel_data_list->data;
  kernel_memory_map_t *kernel_memory_list = kernel_memory_map_lookup1((uint64_t)kernel);
  mem_pair_node *block = kernel_memory_list->head;

  // get kernel cct root node from correlation_id
  uint32_t correlation_id = data.kernel_cct_correlation_id;

  while (block != NULL) {
    /*!
     * @return sampling size for mem handle
     * @ingroup MEM
     * @param[in]   mem     the memory handle
     *
     * @par Availability:
     * - all callbacks
     */
    uint32_t thread_count = GTPin_MemSampleLength(block->mem);
    assert(thread_count > 0);

    uint32_t total = 0, value = 0;
    for (uint32_t tid = 0; tid < thread_count; ++tid) {
      status = GTPin_MemRead(block->mem, tid, sizeof(uint32_t), (char*)(&value), NULL);
      assert(status == GTPINTOOL_STATUS_SUCCESS);
      total += value;
    }

    //block_map_t *bm = block_map_lookup1(data.block_map_root, block->offset);
    //assert(bm != 0);
    uint64_t execution_count = total; // + bm->val 
    //block_map_insert1(data.block_map_root, block->offset, execution_count);

    activityNotify();  
    gpu_activity_t gpu_activity;
    kernelBlockActivityProcess(&gpu_activity, correlation_id, data.loadmap_module_id, block->offset, execution_count);
    block = block->next;
    //how to make offset the primary key within the cct and += the execution value for existing ccts?
  }

  ++(data.call_count);
}



//******************************************************************************
// interface operations
//******************************************************************************


void
opencl_enable_profiling
(
 void
)
{
  ETMSG(OPENCL, "inside enableProfiling");
  initializeInstrumentation();
  knobAddBool("silent_warnings", true);

  /*if (utils::GetEnv("PTI_GEN12") != nullptr) {
    std::cout << "[INFO] Experimental GTPin mode: GEN12" << std::endl;
    KnobAddBool("gen12_1", true);
    }*/

  GTPin_OnKernelBuild(onKernelBuild, NULL);
  GTPin_OnKernelRun(onKernelRun, NULL);
  GTPin_OnKernelComplete(onKernelComplete, NULL);

  GTPIN_Start();
}

