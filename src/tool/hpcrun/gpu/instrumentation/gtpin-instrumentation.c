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

#include <include/gpu-binary.h>
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
// hpctoolkit's interface to gtpin
//******************************************************************************

#include "gtpin-interface.c"



//******************************************************************************
// macros
//******************************************************************************

#define MAX_STR_SIZE 1024
#define KERNEL_SUFFIX ".kernel"
#define ASSERT_GTPIN_STATUS(status) assert(GTPINTOOL_STATUS_SUCCESS == (status))



//******************************************************************************
// local data
//******************************************************************************

// TODO(Aaron): Why there are so many correlation ids
static atomic_ullong correlation_id;

static spinlock_t files_lock = SPINLOCK_UNLOCKED;

static bool gtpin_use_runtime_callstack = false;

static SimdSectionNode *SimdSectionNode_free_list = NULL;
static SimdGroupNode *SimdGroupNode_free_list = NULL;

static __thread uint64_t gtpin_correlation_id = 0;
static __thread uint64_t gtpin_cpu_submit_time = 0;
static __thread gpu_op_ccts_t gtpin_gpu_op_ccts;
static __thread bool gtpin_first = true;



//******************************************************************************
// private operations
//******************************************************************************

// FIXME the asserts in this file should be replaced by fatal error messages

static void
knobAddBool
(
 const char *name,
 bool value
)
{
  GTPinKnob knob = HPCRUN_GTPIN_CALL(KNOB_FindArg, (name));
  assert(knob != NULL);

  KnobValue knob_value;
  knob_value.value._bool = value;
  knob_value.type = KNOB_TYPE_BOOL;

  KNOB_STATUS status = 
    HPCRUN_GTPIN_CALL(KNOB_AddValue, (knob, &knob_value));
  assert(status == KNOB_STATUS_SUCCESS);
}


void
initializeInstrumentation
(
 void
)
{
  if (gtpin_bind() != 0) {
    EEMSG("HPCToolkit fatal error: failed to initialize GTPin "
	  "for instrumentation of GPU binaries");
    exit(1);
  }

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


static size_t
computeHash
(
 const char *mem_ptr,
 size_t mem_size,
 char *name
)
{
  // Compute hash for mem_ptr with mem_size
  unsigned char hash[HASH_LENGTH];
  crypto_hash_compute((const unsigned char *)mem_ptr, mem_size, hash, HASH_LENGTH);

  size_t i;
  size_t used = 0;
  for (i = 0; i < HASH_LENGTH; ++i) {
    used += sprintf(&name[used], "%02x", hash[i]);
  }
  return used;
}


static void
computeBinaryHash
(
 const char *binary,
 size_t binary_size,
 char *file_name
)
{
  size_t used = 0;
  used += sprintf(&file_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&file_name[used], "%s", "/" GPU_BINARY_DIRECTORY "/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  used += computeHash(binary, binary_size, &file_name[used]);
  used += sprintf(&file_name[used], "%s", GPU_BINARY_SUFFIX);
}


static uint32_t
findOrAddKernelModule
(
 GTPinKernel kernel
)
{
  char kernel_name[MAX_STR_SIZE];
  GTPINTOOL_STATUS status;

  status = HPCRUN_GTPIN_CALL(GTPin_KernelGetName,
			     (kernel, MAX_STR_SIZE, kernel_name, NULL));

  ASSERT_GTPIN_STATUS(status);

  uint32_t kernel_elf_size = 0;
  status = HPCRUN_GTPIN_CALL(GTPin_GetElf, (kernel, 0, NULL, &kernel_elf_size));
  ASSERT_GTPIN_STATUS(status);

  char *kernel_elf = (char *)malloc(sizeof(char) * kernel_elf_size);
  status = HPCRUN_GTPIN_CALL(GTPin_GetElf, 
			     (kernel, kernel_elf_size, kernel_elf, NULL));
  ASSERT_GTPIN_STATUS(status);

  // Create file name
  char file_name[PATH_MAX];
  memset(file_name, 0, PATH_MAX);
  computeBinaryHash(kernel_elf, kernel_elf_size, file_name);

  // Write a file if does not exist
  spinlock_lock(&files_lock);
  writeBinary(file_name, kernel_elf, kernel_elf_size);
  spinlock_unlock(&files_lock);

  free(kernel_elf);

  // Compute hash for the kernel name
  char kernel_name_hash[PATH_MAX];
  computeHash(kernel_name, strlen(kernel_name), kernel_name_hash);

  strcat(file_name, ".");
  strncat(file_name, kernel_name_hash, strlen(kernel_name_hash));

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


static SimdSectionNode*
simdsection_node_alloc_helper
(
 SimdSectionNode **free_list
)
{
  SimdSectionNode *first = *free_list;

  if (first) {
    *free_list = first->next;
  } else {
    first = (SimdSectionNode *) hpcrun_malloc_safe(sizeof(SimdSectionNode));
  }

  memset(first, 0, sizeof(SimdSectionNode));
  return first;
}


static void
simdsection_node_free_helper
(
 SimdSectionNode **free_list,
 SimdSectionNode *node
)
{
  node->next = *free_list;
  *free_list = node;
}


static SimdGroupNode*
simdgroup_node_alloc_helper
(
 SimdGroupNode **free_list
)
{
  SimdGroupNode *first = *free_list;

  if (first) {
    *free_list = first->next;
  } else {
    first = (SimdGroupNode *) hpcrun_malloc_safe(sizeof(SimdGroupNode));
  }

  memset(first, 0, sizeof(SimdGroupNode));
  return first;
}


static void
simdgroup_node_free_helper
(
 SimdGroupNode **free_list,
 SimdGroupNode *node
)
{
  node->next = *free_list;
  *free_list = node;
}


static void
kernelBlockActivityTranslate
(
 gpu_activity_t *ga,
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t execution_count,
 uint64_t latency,
 uint64_t active_simd_lanes
)
{
  memset(&ga->details.kernel_block, 0, sizeof(gpu_kernel_block_t));
  ga->details.kernel_block.external_id = correlation_id;
  ga->details.kernel_block.pc.lm_id = (uint16_t)loadmap_module_id;
  ga->details.kernel_block.pc.lm_ip = (uintptr_t)offset;
  ga->details.kernel_block.execution_count = execution_count;
  ga->details.kernel_block.latency = latency;
  ga->details.kernel_block.active_simd_lanes = active_simd_lanes;
  ga->kind = GPU_ACTIVITY_KERNEL_BLOCK;

  cstack_ptr_set(&(ga->next), 0);
}


static void
kernelBlockActivityProcess
(
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t bb_execution_count,
 uint64_t bb_latency_cycles,
 uint64_t bb_active_simd_lanes,
 gpu_activity_channel_t *activity_channel,
 cct_node_t *host_op_node
)
{
  gpu_activity_t ga;
  kernelBlockActivityTranslate(&ga, correlation_id, loadmap_module_id, offset,
      bb_execution_count, bb_latency_cycles, bb_active_simd_lanes);

  ip_normalized_t ip = ga.details.kernel_block.pc;
  cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip); // how to set the ip_norm
  if (cct_child) {
    ga.cct_node = cct_child;
    gpu_operation_multiplexer_push(activity_channel, NULL, &ga);
  }
}


static GTPinMem
addLatencyInstrumentation
(
 GTPinKernel kernel,
 GTPinINS head,
 GTPinINS tail,
 uint32_t *availableLatencyRegisters,
 bool *isInstrumented
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPinMem mem_latency;
  *isInstrumented = false;
  if (GTPin_InsIsEOT(head)) {
    return mem_latency;
  }

  if (GTPin_InsIsChangingIP(tail)) {
    if (head == tail) {
      return mem_latency;
    } else {
      tail = GTPin_InsPrev(tail);
      if (!GTPin_InsValid(tail)) {
        printf("LATENCY instrumentation: instruction is invalid");
        return mem_latency;
      }
    }
  }

  status = GTPin_LatencyInstrumentPre(head);
  if (status != GTPINTOOL_STATUS_SUCCESS) {
    return mem_latency;
  }

  status = GTPin_MemClaim(kernel, sizeof(LatencyDataInternal), &mem_latency);
  if (status != GTPINTOOL_STATUS_SUCCESS) {
    printf("LATENCY instrumentation: failed to claim memory");
    return mem_latency;
  }

  status = GTPin_LatencyInstrumentPost_Mem(tail, mem_latency, *availableLatencyRegisters);
  if (status != GTPINTOOL_STATUS_SUCCESS) {
    return mem_latency;
  }

  if (*availableLatencyRegisters > 0) {
    --(*availableLatencyRegisters);
  }
  *isInstrumented = true;
  return mem_latency;
}


static GTPinMem
addOpcodeInstrumentation
(
 GTPinKernel kernel,
 GTPinINS head
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPinMem mem_opcode = NULL;
  status = HPCRUN_GTPIN_CALL(GTPin_MemClaim, (kernel, sizeof(uint32_t), &mem_opcode));
  ASSERT_GTPIN_STATUS(status);

  status = HPCRUN_GTPIN_CALL(GTPin_OpcodeprofInstrument, (head, mem_opcode));
  ASSERT_GTPIN_STATUS(status);

  return mem_opcode;
}


static SimdSectionNode*
addSimdInstrumentation
(
 GTPinKernel kernel,
 GTPinINS head,
 GTPinINS tail,
 GTPinMem mem_opcode
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  // Divide BBL into sections and groups:
  //
  // Section is a sequence of instructions within BBL, each of which gets the same dynamic
  // input parameters for the SIMD operation calulator. In our case, such a parameter is the
  // flag register value.
  //
  // Group is a sequence of instructions within a section, each of which has the same static
  // input parameters for the SIMD operation calulator. In our case, group consists of instructions
  // having the same SimdProfArgs values.
  GTPinINS sectionHead = head; // head instruction of the current section
  GTPinINS sectionTail = tail;

  uint32_t headId, tailID;
  GTPin_InsID(sectionHead, &headId);
  GTPin_InsID(sectionTail, &tailID);
  SimdSectionNode *sHead = NULL;

  // Iterate through sections within the current BBL
  do
  {
    // Find the upper bound of the current section, and compute number of instructions per each
    // instruction group within the section
    SimdGroupNode *groupHead = NULL;

    bool isSectionEnd = false;
    GTPinINS ins = sectionHead;
    for (; GTPin_InsValid(ins) && !isSectionEnd; ins = GTPin_InsNext(ins)) {
      uint32_t    execMask = GTPin_InsGetExecMask(ins);
      GenPredArgs predArgs = GTPin_InsGetPredArgs(ins);
      bool        maskCtrl = !GTPin_InsIsMaskEnabled(ins);
      uint64_t key = (uint64_t)&execMask + (uint64_t)&predArgs + maskCtrl + (uint64_t)sectionHead;
      simdgroup_map_entry_t *entry = simdgroup_map_lookup(key);
      if (!entry) {
        entry = simdgroup_map_insert(key, maskCtrl, execMask, predArgs);
        SimdGroupNode *gn = simdgroup_node_alloc_helper(&SimdGroupNode_free_list);
        gn->key = key;
        gn->entry = entry;
        gn->next = groupHead;
        groupHead = gn;
      }

      isSectionEnd = GTPin_InsIsFlagModifier(ins);
    }

    // For each element in insCountsPerGroup, create the corresponding SimdProfGroup record,
    // and insert SimdProf instrumentaion at the beginning of the current section
    SimdGroupNode *curr = groupHead;
    SimdGroupNode *next;
    while (curr) {
      next = curr->next;
      GTPinMem mem_simd;
      status = GTPin_MemClaim(kernel, sizeof(uint32_t), &mem_simd); ASSERT_GTPIN_STATUS(status);
      curr->mem_simd = mem_simd;
      
      GenPredArgs pa = simdgroup_entry_getPredArgs(curr->entry);
      status = GTPin_SimdProfInstrument(sectionHead, simdgroup_entry_getMaskCtrl(curr->entry), 
          simdgroup_entry_getExecMask(curr->entry), &pa, mem_simd);
      if (status != GTPINTOOL_STATUS_SUCCESS)	return false;

      simdgroup_map_delete(curr->key);
      curr = next;
    }
    SimdSectionNode *sn = simdsection_node_alloc_helper(&SimdSectionNode_free_list);
    sn->groupHead = groupHead;
    sn->next = sHead;
    sHead = sn;

    sectionHead = ins;
  } while (GTPin_InsValid(sectionHead));
  return sHead;
}


static void
onKernelBuild
(
 GTPinKernel kernel,
 void *v
)
{
  assert(kernel_data_map_lookup((uint64_t)kernel) == 0);

  kernel_data_t kernel_data;
  kernel_data.loadmap_module_id = findOrAddKernelModule(kernel);
  kernel_data.kind = KERNEL_DATA_GTPIN;

  kernel_data_gtpin_block_t *gtpin_block_head = NULL;
  kernel_data_gtpin_block_t *gtpin_block_curr = NULL;
  uint32_t regInst = GTPin_LatencyAvailableRegInstrument(kernel);
  bool hasLatencyInstrumentation;

  for (GTPinBBL block = HPCRUN_GTPIN_CALL(GTPin_BBLHead, (kernel)); 
       HPCRUN_GTPIN_CALL(GTPin_BBLValid, (block)); 
       block = HPCRUN_GTPIN_CALL(GTPin_BBLNext, (block))) {
    GTPinINS head = HPCRUN_GTPIN_CALL(GTPin_InsHead,(block));
    GTPinINS tail = HPCRUN_GTPIN_CALL(GTPin_InsTail,(block));
    assert(HPCRUN_GTPIN_CALL(GTPin_InsValid,(head)));

    GTPinMem mem_latency = addLatencyInstrumentation(kernel, head, tail, &regInst, &hasLatencyInstrumentation);

    GTPinMem mem_opcode = addOpcodeInstrumentation(kernel, head);
    
    SimdSectionNode *shead = addSimdInstrumentation(kernel, head, tail, mem_opcode);
    
    int32_t head_offset = HPCRUN_GTPIN_CALL(GTPin_InsOffset,(head));
    int32_t tail_offset = HPCRUN_GTPIN_CALL(GTPin_InsOffset,(tail));

    kernel_data_gtpin_block_t *gtpin_block = 
      (kernel_data_gtpin_block_t *) hpcrun_malloc(sizeof(kernel_data_gtpin_block_t));
    gtpin_block->head_offset = head_offset;
    gtpin_block->tail_offset = tail_offset;
    gtpin_block->hasLatencyInstrumentation = hasLatencyInstrumentation;
    gtpin_block->mem_latency = mem_latency;
    gtpin_block->mem_opcode = mem_opcode;
    gtpin_block->simd_mem_list = shead;
    gtpin_block->next = NULL;

    if (gtpin_block_head == NULL) {
      gtpin_block_head = gtpin_block;
    } else {
      gtpin_block_curr->next = gtpin_block;
    }
    gtpin_block_curr = gtpin_block;
    
    // while loop that iterates for each instruction in the block and adds an offset entry in map
    int32_t offset = head_offset;
    GTPinINS inst = HPCRUN_GTPIN_CALL(GTPin_InsHead,(block));
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
      inst = HPCRUN_GTPIN_CALL(GTPin_InsNext,(inst));
      offset = HPCRUN_GTPIN_CALL(GTPin_InsOffset,(inst));
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
  HPCRUN_GTPIN_CALL(GTPin_KernelProfilingActive,(kernelExec, 1)); // where is return value?
  ASSERT_GTPIN_STATUS(status);

  createKernelNode((uint64_t)kernelExec);
}


static void
onKernelComplete
(
 GTPinKernelExec kernelExec,
 void *v
)
{
  // FIXME: johnmc thinks this is unsafe to use kernel pointer as correlation id
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
  GTPinKernel kernel = HPCRUN_GTPIN_CALL(GTPin_KernelExec_GetKernel,(kernelExec));
  ETMSG(OPENCL, "onKernelComplete starting. Lookup: kernel: %"PRIu64"", (uint64_t)kernel);
  assert(kernel_data_map_lookup((uint64_t)kernel) != 0);

  kernel_data_map_entry_t *kernel_data_map_entry = kernel_data_map_lookup((uint64_t)kernel);
  assert(kernel_data_map_entry != NULL);

  kernel_data_t kernel_data = kernel_data_map_entry_kernel_data_get(kernel_data_map_entry);
  assert(kernel_data.kind == KERNEL_DATA_GTPIN);

  kernel_data_gtpin_t *kernel_data_gtpin = (kernel_data_gtpin_t *)kernel_data.data; 
  kernel_data_gtpin_block_t *block = kernel_data_gtpin->block;

  while (block != NULL) {
    uint32_t thread_count = HPCRUN_GTPIN_CALL(GTPin_MemSampleLength,(block->mem_opcode));
    assert(thread_count > 0);

    uint64_t bb_exec_count = 0, bb_latency_cycles = 0, bb_active_simd_lanes = 0;
    uint32_t opcodeData = 0, simdData = 0;
    LatencyDataInternal latencyData;
    for (uint32_t tid = 0; tid < thread_count; ++tid) {
      // execution frequency
      status = HPCRUN_GTPIN_CALL(GTPin_MemRead,
          (block->mem_opcode, tid, sizeof(uint32_t), (char*)(&opcodeData), NULL));
      ASSERT_GTPIN_STATUS(status);
      bb_exec_count += opcodeData;

      // latency cycles
      // latency calculation of some basicblocks are skipped due to timer overflow
      // latencyData has _skipped and _freq(total exec count) values as well
      // if latencyData has _freq, do we need to do opcodeProf instrumentation?
      status = GTPin_MemRead(block->mem_latency, tid, sizeof(LatencyDataInternal), (char*)&latencyData, NULL);
      ASSERT_GTPIN_STATUS(status);
      bb_latency_cycles += latencyData._cycles;

      // active simd lanes
      SimdSectionNode *shead = block->simd_mem_list;
      SimdSectionNode *curr_s = shead;
      SimdSectionNode *next_s;
      while (curr_s) {
        next_s = curr_s->next;
        SimdGroupNode *curr_g = curr_s->groupHead;
        SimdGroupNode *next_g;
        while (curr_g) {
          next_g = curr_g->next;
          status = GTPin_MemRead(curr_g->mem_simd, tid, sizeof(uint32_t), (char*)&simdData, NULL);
          bb_active_simd_lanes += simdData;
          simdgroup_node_free_helper(&SimdGroupNode_free_list, curr_g);
          curr_g = next_g;
        }
        simdsection_node_free_helper(&SimdSectionNode_free_list, curr_s);
        curr_s = next_s;
      }
    }

    kernel_data_gtpin_inst_t *inst = block->inst;
    while (inst != NULL) {
      kernelBlockActivityProcess(correlation_id, kernel_data.loadmap_module_id,
        inst->offset, bb_exec_count, bb_latency_cycles, bb_active_simd_lanes, activity_channel, host_op_node);
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

  HPCRUN_GTPIN_CALL(GTPin_OnKernelBuild,(onKernelBuild, NULL));
  HPCRUN_GTPIN_CALL(GTPin_OnKernelRun,(onKernelRun, NULL));
  HPCRUN_GTPIN_CALL(GTPin_OnKernelComplete,(onKernelComplete, NULL));

  HPCRUN_GTPIN_CALL(GTPIN_Start,());
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
