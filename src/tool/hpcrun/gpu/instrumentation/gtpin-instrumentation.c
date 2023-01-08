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
// Copyright ((c)) 2002-2022, Rice University
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
#include <ged_ops.h>   // GTPin_InsGetExecSize
#include <inttypes.h>
#include <math.h>



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
#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>

#include <lib/prof-lean/crypto-hash.h>

#include "gtpin-correlation-id-map.h"
#include "gtpin-instrumentation.h"
#include "kernel-data.h"
#include "kernel-data-map.h"

//******************************************************************************
// macros
//******************************************************************************

#define STUB 0

#define GTPIN_KNOB_AVAILABLE 1


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
// type declaration
//******************************************************************************

// We assume that the OnKernelRun callback from gtpin and the
// subscriber callback from level 0 or opencl
// for each kernel are executed in the same order.
// Suppose we have kernel K1, K2, K3 launched in order.
// It is possible that OnKernelRun(K2) is before subscriber(K1).
// However, OnKernelRun(K1) < OnKernelRun(K2) < OnKernelRun(K3)
// and subscriber(K1) < subscriber(K2) < subscriber(K3).
// Therefore, we can use a queue (implemented as a linked list) to match
// the gpu cct node (from the subscriber callback) with the correlation id
// (from the gtpin OnKernelRun callback)
typedef struct gtpin_correlation_data {
  uint64_t correlation_id;
  uint64_t cpu_submit_time;
  struct gtpin_correlation_data * next;
} gtpin_correlation_data_t;

//******************************************************************************
// local data
//******************************************************************************

static bool gtpin_use_runtime_callstack = false;

static bool simd_knob = false;
static bool latency_knob = false;
static bool count_knob = false;

static SimdSectionNode *SimdSectionNode_free_list = NULL;
static SimdGroupNode *SimdGroupNode_free_list = NULL;

// variables for instruction latency distribution
// for GEN9
int W_min = 4;  // min cycles for a predictable instruction
int C = 2;      // # complex weight

static bool gtpin_is_enabled = false;

// The gtpin correlation data list is thread local
// as there is no ordering guarantee among threads
static __thread gtpin_correlation_data_t head;
static __thread gtpin_correlation_data_t* tail = NULL;



//******************************************************************************
// private operations
//******************************************************************************

// FIXME the asserts in this file should be replaced by fatal error messages

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
}


static void
createKernelNode
(
 uint64_t correlation_id
)
{
  uint64_t cpu_submit_time = hpcrun_nanotime();

  if (gtpin_use_runtime_callstack) {
    // allocate a new node and put it into the thread local list
    gtpin_correlation_data_t* data = (gtpin_correlation_data_t*) malloc(sizeof(gtpin_correlation_data_t));

    if (tail == 0) {
      // lazy initialization of correlation list so it will be initialized for each thread
      head.next = NULL;
      tail = &head;
    }
    data->correlation_id = correlation_id;
    data->cpu_submit_time = cpu_submit_time;
    data->next = NULL;
    tail->next = data;
    tail = data;
  } else {
    // If we do not use opencl or level 0 call path,
    // then we just take a call path right now
    // and generate the correlation id record
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

static uint32_t
findOrAddKernelModule
(
 GTPinKernel kernel
)
{
  uint32_t kernel_name_len;
  GTPINTOOL_STATUS status;
  status = HPCRUN_GTPIN_CALL(GTPin_KernelGetName,
			     (kernel, 0, NULL, &kernel_name_len));

  assert(status == GTPINTOOL_STATUS_SUCCESS);

  char *kernel_name = (char *) malloc(kernel_name_len+1);
  status = HPCRUN_GTPIN_CALL(GTPin_KernelGetName,
			     (kernel, kernel_name_len, kernel_name, NULL));

  assert(status == GTPINTOOL_STATUS_SUCCESS);

  uint32_t kernel_elf_size = 0;
  status = HPCRUN_GTPIN_CALL(GTPin_GetElf, (kernel, 0, NULL, &kernel_elf_size));
  ASSERT_GTPIN_STATUS(status);

  char *kernel_elf = (char *)malloc(sizeof(char) * kernel_elf_size);
  status = HPCRUN_GTPIN_CALL(GTPin_GetElf,
			     (kernel, kernel_elf_size, kernel_elf, NULL));
  ASSERT_GTPIN_STATUS(status);

  // Create file name
  char file_name[CRYPTO_HASH_STRING_LENGTH];
  memset(file_name, 0, CRYPTO_HASH_STRING_LENGTH);
  crypto_compute_hash_string(kernel_elf, kernel_elf_size, file_name,
			     sizeof(file_name));

  // Compute hash for the kernel name
  char kernel_name_hash[CRYPTO_HASH_STRING_LENGTH];
  crypto_compute_hash_string(kernel_name, strlen(kernel_name),
			     kernel_name_hash, sizeof(kernel_name_hash));

  char path[PATH_MAX];
  memset(path, 0, PATH_MAX);
  gpu_binary_path_generate(file_name, path);

  // Write the GPU binary if it doesn't exist
  gpu_binary_store(path, kernel_elf, kernel_elf_size);

  free(kernel_elf);
  free(kernel_name);

  // extend the GPU binary name with the kernel hash
  // for the module name
  strcat(path, ".");
  strncat(path, kernel_name_hash, strlen(kernel_name_hash));

  uint32_t module_id = gpu_binary_loadmap_insert(path, true /* mark_used */);

#if 0
  opencl_kernel_loadmap_map_insert(kernel_name_id, module_id);
#endif

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

  //printf("simdsection. use: %p\n", first);
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
  //printf("simdsection. freed: %p\n", node);
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

  //printf("simdgroup. use: %p\n", first);
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
  //printf("simdgroup. freed: %p\n", node);
  node->next = *free_list;
  *free_list = node;
}


static void
kernelActivityTranslate
(
 gpu_activity_t *ga,
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 bool isInstruction,
 uint32_t bb_instruction_count,
 uint64_t execution_count,
 uint64_t latency,
 uint64_t active_simd_lanes,
 uint64_t total_simd_lanes,
 uint64_t scalar_simd_loss
)
{
  memset(&ga->details.kernel_block, 0, sizeof(gpu_kernel_block_t));
  ga->details.kernel_block.external_id = correlation_id;
  ga->details.kernel_block.pc.lm_id = (uint16_t)loadmap_module_id;
  ga->details.kernel_block.pc.lm_ip = (uintptr_t)offset;
  ga->details.kernel_block.instruction = isInstruction;
  ga->details.kernel_block.bb_instruction_count = bb_instruction_count;
  ga->details.kernel_block.execution_count = execution_count;
  ga->details.kernel_block.latency = latency;
  ga->details.kernel_block.active_simd_lanes = active_simd_lanes;
  ga->details.kernel_block.total_simd_lanes = total_simd_lanes;
  ga->details.kernel_block.scalar_simd_loss = scalar_simd_loss;
  ga->kind = GPU_ACTIVITY_KERNEL_BLOCK;

  cstack_ptr_set(&(ga->next), 0);
}


static void
kernelInstructionActivityProcess
(
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint64_t bb_execution_count,
 uint64_t inst_latency,
 gpu_activity_channel_t *activity_channel,
 cct_node_t *host_op_node
)
{
  gpu_activity_t ga;
  kernelActivityTranslate(&ga, correlation_id, loadmap_module_id, offset, true,
      0, bb_execution_count, inst_latency, 0, 0, 0);

  ip_normalized_t ip = ga.details.kernel_block.pc;
  cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip, true); // how to set the ip_norm
  if (cct_child) {
    ga.cct_node = cct_child;
    gpu_operation_multiplexer_push(activity_channel, NULL, &ga);
  }
}


static void
kernelBlockActivityProcess
(
 uint64_t correlation_id,
 uint32_t loadmap_module_id,
 uint64_t offset,
 uint32_t bb_instruction_count,
 uint64_t bb_execution_count,
 uint64_t bb_latency_cycles,
 uint64_t bb_active_simd_lanes,
 uint64_t bb_total_simd_lanes,
 uint64_t scalar_simd_loss,
 gpu_activity_channel_t *activity_channel,
 cct_node_t *host_op_node
)
{
  gpu_activity_t ga;
  kernelActivityTranslate(&ga, correlation_id, loadmap_module_id, offset, false,
      bb_instruction_count, bb_execution_count, bb_latency_cycles, bb_active_simd_lanes, bb_total_simd_lanes, scalar_simd_loss);

  ip_normalized_t ip = ga.details.kernel_block.pc;

  //FIXME? Aaron had the last argument as true
  cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip, false); // how to set the ip_norm
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
  if (HPCRUN_GTPIN_CALL(GTPin_InsIsEOT, (head))) {
    return mem_latency;
  }

  if (HPCRUN_GTPIN_CALL(GTPin_InsIsChangingIP, (tail))) {
    if (head == tail) {
      return mem_latency;
    } else {
      tail = HPCRUN_GTPIN_CALL(GTPin_InsPrev,(tail));
      if (!HPCRUN_GTPIN_CALL(GTPin_InsValid, (tail))) {
        printf("LATENCY instrumentation: instruction is invalid");
        return mem_latency;
      }
    }
  }

  status = HPCRUN_GTPIN_CALL(GTPin_LatencyInstrumentPre, (head));
  if (status != GTPINTOOL_STATUS_SUCCESS) {
    return mem_latency;
  }

  status = HPCRUN_GTPIN_CALL(GTPin_MemClaim, (kernel, sizeof(LatencyDataInternal), &mem_latency));
  if (status != GTPINTOOL_STATUS_SUCCESS) {
    printf("LATENCY instrumentation: failed to claim memory");
    return mem_latency;
  }

  status = HPCRUN_GTPIN_CALL(GTPin_LatencyInstrumentPost_Mem, (tail, mem_latency, *availableLatencyRegisters));
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
 GTPinMem mem_opcode,
 uint32_t *bb_scalar_instructions
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
  *bb_scalar_instructions = 0;
  SimdSectionNode *sHead = NULL;

  // Iterate through sections within the current BBL
  do
  {
    // Find the upper bound of the current section, and compute number of instructions per each
    // instruction group within the section
    SimdGroupNode *groupHead = NULL;

    bool isSectionEnd = false;
    GTPinINS ins = sectionHead;
    for (; HPCRUN_GTPIN_CALL(GTPin_InsValid, (ins)) && !isSectionEnd; ins = HPCRUN_GTPIN_CALL(GTPin_InsNext, (ins))) {
      uint32_t execSize = HPCRUN_GTPIN_CALL(GTPin_InsGetExecSize, (ins));
      if (execSize == 1) {
        (*bb_scalar_instructions)++;
      }
      uint32_t    execMask = HPCRUN_GTPIN_CALL(GTPin_InsGetExecMask,(ins));
      GenPredArgs predArgs = HPCRUN_GTPIN_CALL(GTPin_InsGetPredArgs,(ins));
      bool        maskCtrl = !HPCRUN_GTPIN_CALL(GTPin_InsIsMaskEnabled,(ins));
      uint64_t key = (uint64_t)execMask + (uint64_t)&predArgs + maskCtrl + (uint64_t)sectionHead;
      simdgroup_map_entry_t *entry = simdgroup_map_lookup(key);
      if (!entry) {
        entry = simdgroup_map_insert(key, maskCtrl, execMask, predArgs);
        SimdGroupNode *gn = simdgroup_node_alloc_helper(&SimdGroupNode_free_list);
        gn->key = key;
        gn->entry = entry;
        gn->next = groupHead;
        groupHead = gn;
      } else {
        simdgroup_entry_increment_inst_count(entry);
      }

      isSectionEnd = HPCRUN_GTPIN_CALL(GTPin_InsIsFlagModifier, (ins));
    }

    // For each element in insCountsPerGroup, create the corresponding SimdProfGroup record,
    // and insert SimdProf instrumentation at the beginning of the current section
    SimdGroupNode *curr = groupHead;
    SimdGroupNode *next;
    int count = 0;
    while (curr) {
      count++;
      next = curr->next;
      GTPinMem mem_simd;
      status = HPCRUN_GTPIN_CALL(GTPin_MemClaim, (kernel, sizeof(uint32_t), &mem_simd)); ASSERT_GTPIN_STATUS(status);
      curr->mem_simd = mem_simd;

      GenPredArgs pa = simdgroup_entry_getPredArgs(curr->entry);
      status = HPCRUN_GTPIN_CALL(GTPin_SimdProfInstrument, (sectionHead, simdgroup_entry_getMaskCtrl(curr->entry),
          simdgroup_entry_getExecMask(curr->entry), &pa, mem_simd));
      if (status != GTPINTOOL_STATUS_SUCCESS)	return false;
      curr->instCount = simdgroup_entry_getInst(curr->entry);
      simdgroup_map_delete(curr->key);
      curr = next;
    }
    SimdSectionNode *sn = simdsection_node_alloc_helper(&SimdSectionNode_free_list);
    sn->groupHead = groupHead;
    sn->next = sHead;
    sHead = sn;

    sectionHead = ins;
  } while (HPCRUN_GTPIN_CALL(GTPin_InsValid, (sectionHead)));
  return sHead;
}


static bool
isPredictable
(
 char *asm_str
)
{
  if (strstr(asm_str, "send")) {
    return false;
  }
  return true;
}


static bool
isComplex
(
 char *asm_str
)
{
  int SIZE = 10;
  const char* const matches[10] = {"INV", "LOG", "EXP", "SQRT", "RSQ", "POW", "SIN", "COS", "INT DIV", "INVM/RSQRTM"};
  for (int x = 0; x < SIZE; x++) {
    if (strstr(asm_str, matches[x])) {
      return true;
    }
  }
  return false;
}


static float
Div
(
 float n,
 float d
)
{
  if (d) {
    return n / d;
  } else {
    return 0;
  }
}


static void
calculateInstructionWeights
(
  kernel_data_gtpin_block_t *bb
)
{
  int unpredictable_instructions = 0, sum_predictable_latency = 0;
  float bb_avg_latency = Div(bb->aggregated_latency, bb->execution_count);

  for (struct kernel_data_gtpin_inst *inst = bb->inst; inst != NULL; inst = inst->next) {
      int size = (inst->execSize <= 4) ? 4 : inst->execSize;
      inst->W_ins = size * W_min / 4;
      if (inst->isPredictable) {
        if (inst->isComplex) {
          inst->W_ins *= C;
        }
        sum_predictable_latency += inst->W_ins;
      }
      else {
        unpredictable_instructions += 1;
      }
  }

  float W_unpredictable_instruction = Div(bb_avg_latency - sum_predictable_latency, unpredictable_instructions);

  float sum_instruction_weights = 0;
  for (struct kernel_data_gtpin_inst *inst = bb->inst; inst != NULL; inst = inst->next) {
    if (!inst->isPredictable && (W_unpredictable_instruction > 0)) {
      inst->W_ins = W_unpredictable_instruction;
    }
    sum_instruction_weights += inst->W_ins;
  }

  float normalization_factor = Div(bb_avg_latency, sum_instruction_weights);

  for (struct kernel_data_gtpin_inst *inst = bb->inst; inst != NULL; inst = inst->next) {
    inst->W_ins *= normalization_factor;
    inst->aggregated_latency = round(inst->W_ins * bb->execution_count);
  }
}


static void
onKernelBuild
(
 GTPinKernel kernel,
 void *v
)
{
#if STUB
  return;
#endif

  assert(kernel_data_map_lookup((uint64_t)kernel) == 0);

  kernel_data_t kernel_data;
  kernel_data.loadmap_module_id = findOrAddKernelModule(kernel);
  kernel_data.kind = KERNEL_DATA_GTPIN;

  kernel_data_gtpin_block_t *gtpin_block_head = NULL;
  kernel_data_gtpin_block_t *gtpin_block_curr = NULL;

  bool hasLatencyInstrumentation = false;
  uint32_t regInst;
  if (latency_knob) {
    regInst = HPCRUN_GTPIN_CALL(GTPin_LatencyAvailableRegInstrument, (kernel));
  }

  for (GTPinBBL block = HPCRUN_GTPIN_CALL(GTPin_BBLHead, (kernel));
       HPCRUN_GTPIN_CALL(GTPin_BBLValid, (block));
       block = HPCRUN_GTPIN_CALL(GTPin_BBLNext, (block))) {
    GTPinINS head = HPCRUN_GTPIN_CALL(GTPin_InsHead,(block));
    GTPinINS tail = HPCRUN_GTPIN_CALL(GTPin_InsTail,(block));
    assert(HPCRUN_GTPIN_CALL(GTPin_InsValid,(head)));

    uint32_t bb_scalar_instructions = 0;

    GTPinMem mem_latency;
    if (latency_knob) {
      mem_latency = addLatencyInstrumentation(kernel, head, tail, &regInst, &hasLatencyInstrumentation);
    }

    GTPinMem mem_opcode;
    // latency instrumentation can also retrieve counts
    if ((count_knob && !hasLatencyInstrumentation) || (latency_knob && !hasLatencyInstrumentation)) {
      mem_opcode = addOpcodeInstrumentation(kernel, head);
    }

    SimdSectionNode *shead;
    if (simd_knob) {
      shead = addSimdInstrumentation(kernel, head, tail, mem_opcode, &bb_scalar_instructions);
    }

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
    gtpin_block->scalar_instructions = bb_scalar_instructions;
    gtpin_block->next = NULL;

    if (gtpin_block_head == NULL) {
      gtpin_block_head = gtpin_block;
    } else {
      gtpin_block_curr->next = gtpin_block;
    }
    gtpin_block_curr = gtpin_block;

    // while loop that iterates for each instruction in the block and adds an offset entry in map
    int32_t offset = head_offset;
    int bb_instruction_count = 0;
    GTPinINS inst = HPCRUN_GTPIN_CALL(GTPin_InsHead, (block));
    kernel_data_gtpin_inst_t *gtpin_inst_curr = NULL;
    while (offset <= tail_offset && offset != -1) {
      bb_instruction_count++;
      kernel_data_gtpin_inst_t *gtpin_inst = (kernel_data_gtpin_inst_t *)hpcrun_malloc(sizeof(kernel_data_gtpin_inst_t));
      ged_ins_t gedInst = HPCRUN_GTPIN_CALL(GTPin_InsGED, (inst));
      uint32_t asm_str_size;
      HPCRUN_GTPIN_CALL(GTPin_InsDisasm, (&gedInst, 0, NULL, &asm_str_size));
      char *asm_str = (char*) hpcrun_malloc(asm_str_size);
      HPCRUN_GTPIN_CALL(GTPin_InsDisasm, (&gedInst, asm_str_size, asm_str, NULL));

      gtpin_inst->offset = offset;
      gtpin_inst->isPredictable = isPredictable(asm_str);
      gtpin_inst->isComplex = isComplex(asm_str);
      gtpin_inst->execSize = HPCRUN_GTPIN_CALL(GTPin_InsGetExecSize, (inst));

      if (gtpin_inst_curr == NULL) {
        gtpin_block_curr->inst = gtpin_inst;
      } else {
        gtpin_inst_curr->next = gtpin_inst;
      }
      gtpin_inst_curr = gtpin_inst;
      inst = HPCRUN_GTPIN_CALL(GTPin_InsNext,(inst));
      offset = HPCRUN_GTPIN_CALL(GTPin_InsOffset,(inst));
    }
    gtpin_block_curr->instruction_count = bb_instruction_count;
  }

  if (gtpin_block_head != NULL) {
    kernel_data_gtpin_t *kernel_data_gtpin = (kernel_data_gtpin_t *)hpcrun_malloc(sizeof(kernel_data_gtpin_t));
    kernel_data_gtpin->kernel_id = (uint64_t)kernel;
    kernel_data_gtpin->simd_width = HPCRUN_GTPIN_CALL(GTPin_KernelGetSIMD, (kernel));
    kernel_data_gtpin->block = gtpin_block_head;
    kernel_data.data = kernel_data_gtpin;
    kernel_data_map_insert((uint64_t)kernel, kernel_data);
  }

  // add these details to cct_node. If that's not needed, we can create the kernel_cct in onKernelComplete
  ETMSG(OPENCL, "onKernelBuild complete. Inserted key: %"PRIu64 "",(uint64_t)kernel);
}


static void
onKernelRun
(
 GTPinKernelExec kernelExec,
 void *v
)
{
#if STUB
  return;
#endif

  ETMSG(OPENCL, "onKernelRun starting. Inserted: correlation %"PRIu64"", (uint64_t)kernelExec);

  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  status = HPCRUN_GTPIN_CALL(GTPin_KernelProfilingActive,(kernelExec, 1));
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
#if STUB
  return;
#endif

  // FIXME: johnmc thinks this is unsafe to use kernel pointer as correlation id
  uint64_t correlation_id = (uint64_t)kernelExec;

  gtpin_correlation_id_map_entry_t *entry =
    gtpin_correlation_id_map_lookup(correlation_id);

  ETMSG(OPENCL, "onKernelComplete starting. Lookup: correlation %"PRIu64", result %p", correlation_id, entry);

  if (entry == NULL) {
    // XXX(Keren): the opencl/level zero api's kernel launch is not wrapped
    // or instrumentation is turned off
    return;
  }

  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);

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
    uint32_t thread_count;

    if (block->hasLatencyInstrumentation) {
      thread_count = HPCRUN_GTPIN_CALL(GTPin_MemSampleLength,(block->mem_latency));
    } else {
      // simd, count and latency probes all need mem_opcode probes
      // simd: needs mem_opcode for bb_exec_count calculation
      // count: same as above
      // latency: needs mem_opcode for all basic blocks where latency probes couldn't be added
      thread_count = HPCRUN_GTPIN_CALL(GTPin_MemSampleLength,(block->mem_opcode));
    }
    assert(thread_count > 0);

    uint64_t bb_exec_count = 0, bb_latency_cycles = 0, bb_active_simd_lanes = 0;
    uint32_t opcodeData = 0, simdData = 0;
    LatencyDataInternal latencyData;
    SimdSectionNode *shead, *curr_s, *next_s;
    shead = block->simd_mem_list;
    SimdGroupNode *curr_g, *next_g;

    for (uint32_t tid = 0; tid < thread_count; ++tid) {

      // latency cycles
      // latency calculation of some basicblocks are skipped due to timer overflow
      if (latency_knob && block->hasLatencyInstrumentation) {
        status = HPCRUN_GTPIN_CALL(GTPin_MemRead, (block->mem_latency, tid, sizeof(LatencyDataInternal), (char*)&latencyData, NULL));
        ASSERT_GTPIN_STATUS(status);
        bb_latency_cycles += latencyData._cycles;
        // execution_count
        bb_exec_count += latencyData._freq;
      }

      // execution_count
      if (count_knob && !block->hasLatencyInstrumentation) {
        status = HPCRUN_GTPIN_CALL(GTPin_MemRead,
            (block->mem_opcode, tid, sizeof(uint32_t), (char*)(&opcodeData), NULL));
        ASSERT_GTPIN_STATUS(status);
        bb_exec_count += opcodeData;
      }

      // active simd lanes
      if (simd_knob) {
        curr_s = shead;
        while (curr_s) {
          next_s = curr_s->next;
          curr_g = curr_s->groupHead;
          while (curr_g) {
            next_g = curr_g->next;
            status = HPCRUN_GTPIN_CALL(GTPin_MemRead,(curr_g->mem_simd, tid, sizeof(uint32_t), (char*)&simdData, NULL));
            bb_active_simd_lanes += (simdData * curr_g->instCount);
            curr_g = next_g;
          }
          curr_s = next_s;
        }
      }
    }
#if 0
    // cleanup
    if (simd_knob) {
      curr_s = shead;
      while (curr_s) {
        next_s = curr_s->next;
        curr_g = curr_s->groupHead;
        while (curr_g) {
          next_g = curr_g->next;
          simdgroup_node_free_helper(&SimdGroupNode_free_list, curr_g);
          curr_g = next_g;
        }
        simdsection_node_free_helper(&SimdSectionNode_free_list, curr_s);
        curr_s = next_s;
      }
    }
#endif
    // scalar simd loss
    uint64_t bb_total_simd_lanes = 0, scalar_simd_loss = 0;

    // we need count_knob to get the bb_exec_count
    if (simd_knob) {
      bb_total_simd_lanes = bb_exec_count * kernel_data_gtpin->simd_width * block->instruction_count;
      scalar_simd_loss = block->scalar_instructions * bb_exec_count * (kernel_data_gtpin->simd_width - 1);
    }

    if (latency_knob) {
      block->aggregated_latency = bb_latency_cycles;
      block->execution_count = bb_exec_count;
      calculateInstructionWeights(block);
    }

    kernel_data_gtpin_inst_t *inst = block->inst;
    kernelBlockActivityProcess(correlation_id, kernel_data.loadmap_module_id,
        inst->offset, block->instruction_count, block->execution_count, block->aggregated_latency,
        bb_active_simd_lanes, bb_total_simd_lanes, scalar_simd_loss,
        activity_channel, host_op_node);
    while (inst != NULL) {
      kernelInstructionActivityProcess(correlation_id, kernel_data.loadmap_module_id,
        inst->offset, bb_exec_count, inst->aggregated_latency,
        activity_channel, host_op_node);
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
gtpin_instrumentation_options
(
 gpu_instrumentation_t *inst_options
)
{
  bool enabled = false;
  if (inst_options->count_instructions) {
    count_knob = true;
    enabled = true;
  }
  if (inst_options->attribute_latency) {
    latency_knob = true;
    enabled = true;
  }
  if (inst_options->analyze_simd) {
    simd_knob = true;
    enabled = true;
  }
  if (enabled) {
    gtpin_enable_profiling();
  }
}


void
gtpin_enable_profiling
(
 void
)
{
  ETMSG(OPENCL, "inside enableProfiling");
  initializeInstrumentation();

#if GTPIN_KNOB_AVAILABLE
  gtpin_knob_bool("silent_warnings", true);
  gtpin_knob_bool("no_empty_profile_dir", true);
#endif

  // Use opencl/level zero runtime stack
  gtpin_use_runtime_callstack = true;

  HPCRUN_GTPIN_CALL(GTPin_OnKernelBuild,(onKernelBuild, NULL));
  HPCRUN_GTPIN_CALL(GTPin_OnKernelRun,(onKernelRun, NULL));
  HPCRUN_GTPIN_CALL(GTPin_OnKernelComplete,(onKernelComplete, NULL));

  HPCRUN_GTPIN_CALL(GTPIN_Start,());

  gtpin_is_enabled = true;
}


bool
gtpin_enabled
(
 void
)
{
  return gtpin_is_enabled == true;
}


void
gtpin_produce_runtime_callstack
(
 gpu_op_ccts_t *gpu_op_ccts
)
{
  // We assume that the gtpin OnKernelRun callback has been invoked
  // before the level 0 / opencl subscriber callback.
  // So, the correlation data link list should never be empty
  assert(head.next != NULL);

  // We get the first gtpin correlation data node
  // as we assume that the OnKernelRun and level 0 / opnecl subscriber callback
  gtpin_correlation_data_t* data = head.next;
  gtpin_correlation_id_map_insert(data->correlation_id, gpu_op_ccts, gpu_activity_channel_get(), data->cpu_submit_time);
  if (data->next == NULL) {
    tail = &head;
  }
  head.next = data->next;
  free(data);
}
