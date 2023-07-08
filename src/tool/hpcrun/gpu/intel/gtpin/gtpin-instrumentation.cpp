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
// Copyright ((c)) 2002-2023, Rice University
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

//***************************************************************************
//
// File:
//   gtpin-instrumentation.cpp
//
// Purpose:
//   define the C++ helper library interface for interacting with GTPin
//
//   NOTE: some of the code in this file was adapted from Intel's gtpin samples
//   distributed under MIT license
/*========================== begin_copyright_notice ============================
Copyright (C) 2016-2022 Intel Corporation

SPDX-License-Identifier: MIT
============================= end_copyright_notice ===========================*/
//
//***************************************************************************

//*****************************************************************************
// system include files
//*****************************************************************************

#include <string>
#include <vector>
#include <map>
#include <string.h>
#include <math.h>
#include <set>

//*****************************************************************************
// gtpin include files
//*****************************************************************************

#include <api/gtpin_api.h>
#include <gtpin.h>

//*****************************************************************************
// local include files
//*****************************************************************************

#include "gtpin-instrumentation.h"

extern "C"
{
#include <hpcrun/gpu/gpu-correlation.h>
#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/safe-sampling.h>
#include <include/gpu-binary.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-operation-multiplexer.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/crypto-hash.h>
#include <hpcrun/messages/messages.h>
#include <monitor.h>
};

#include <mutex>

#include <lib/profile/util/locked_unordered.hpp>

//*****************************************************************************
// import namespaces
//*****************************************************************************

using namespace gtpin;
using namespace hpctoolkit::util;



//*****************************************************************************
// type declarations
//*****************************************************************************

class MyHashFunction {
public:
    std::size_t operator()(const std::pair<uint16_t, uintptr_t> &pair) const {
        return std::hash<uint16_t>()(pair.first) ^ std::hash<uintptr_t>()(pair.second);
    }
};


class MyHashFunctionKernelId {
public:
  std::size_t operator()(const GtKernelId &pair) const {
    return std::hash<uint16_t>()(pair);
  }
};


struct CorrelationData {
  std::string correlation_id;
  uint64_t cpu_submit_time;
  gpu_op_ccts_t op_ccts;
  gpu_activity_channel_t *activity_channel;
  CorrelationData *next;
};


struct OpcodeRecord {
  uint32_t freq;
};


struct SimdRecord {
  uint64_t opCount;
};


struct LatencyRecord {
  uint64_t cycles;
  uint32_t freq;
};


struct Instruction {
  uint32_t execSize;
  bool isPredictable;
  bool isComplex;
  float wIns;
  uint64_t latency;
  int32_t offset;

  Instruction(const IGtIns &ins);
};


struct SimdGroup {
  InsId sectionHeadId;
  std::vector<Instruction> instructions;

  bool maskCtrl;
  uint32_t execMask;
  GtPredicate predicate;
  bool isSendIns;

  SimdGroup(const IGtIns &ins) : sectionHeadId(ins.Id()),
				 maskCtrl(!ins.IsWriteMaskEnabled()),
				 execMask(ins.ExecMask().Bits()),
				 predicate(ins.Predicate()),
				 isSendIns(ins.IsSendMessage()) {}
  inline bool operator<(const SimdGroup &other) const {
    return std::make_tuple(maskCtrl, execMask, predicate, isSendIns) <
      std::make_tuple(other.maskCtrl, other.execMask, other.predicate, other.isSendIns);
  }
};


struct BasicBlock {
  std::vector<Instruction> instructions;
  std::vector<SimdGroup> simdGroups;
  uint64_t executionCount;
  uint64_t latency;
  uint32_t simdWidth;
  uint64_t activeSimdLanes;

  BasicBlock(){};

  BasicBlock(const IGtCfg &cfg, const IGtBbl &bbl, uint32_t simdWidthIn, bool simd);
};


class KernelProfile
{
public:
    uint32_t loadmapModuleId;
    GtProfileArray opcodeProfile;
    GtProfileArray simdProfile;
    GtProfileArray latencyProfile;
    std::vector<BasicBlock> basicBlocks;
    uint32_t simdGroupsCount;

    GtReg _addrReg;
    GtReg _dataReg; 
    GtReg _timeReg;

    void GeneratePostCodeMemBound
    (GtGenProcedure &proc, const IGtGenCoder &coder,
     const GtProfileArray &profileArray, uint32_t recordNum);
    void GeneratePreCode(GtGenProcedure &proc, const IGtGenCoder &coder);
    bool GeneratePostCodeRegBound
    (GtGenProcedure &postProc, GtGenProcedure &finiProc,
     const IGtGenCoder &coder, const GtProfileArray &profileArray,
     uint32_t recordNum);

    KernelProfile(uint32_t loadmapModuleIdIn) : loadmapModuleId(loadmapModuleIdIn) {
      simdGroupsCount = 0;
    }

    void instrument(IGtKernelInstrument &);
    void gatherMetrics(IGtKernelDispatch &, CorrelationData);
};


class GTPinInstrumentation : public IGtTool {
public:
  const char *Name() const override { return "GTPin Instrumentation"; }
  uint32_t ApiVersion() const override { return GTPIN_API_VERSION; }

  bool Register();

  static GTPinInstrumentation *Instance()
  {
    static GTPinInstrumentation instance;
    return &instance;
  }

  void OnKernelBuild(IGtKernelInstrument &);
  void OnKernelRun(IGtKernelDispatch &);
  void OnKernelComplete(IGtKernelDispatch &);
};



//*****************************************************************************
// local data
//*****************************************************************************

static thread_local CorrelationData correlation_head;
static thread_local CorrelationData *correlation_tail = NULL;
static thread_local std::map<std::string, CorrelationData> correlation_map;

static locked_unordered_map<std::pair<uint16_t, uintptr_t>, BasicBlock, std::mutex, MyHashFunction> block_map;
static locked_unordered_map<GtKernelId, KernelProfile, std::mutex, MyHashFunctionKernelId> _kernels;
static locked_unordered_map<GtKernelId, uint32_t, std::mutex, MyHashFunctionKernelId> hash_map;

static bool count_knob = false;
static bool collect_latency = false;
static bool latency_knob = false;
static bool simd_knob = false;

static bool gtpin_modern = false; // later than 3.0

static gtpin_hpcrun_api_t *gtpin_hpcrun_api;



//*****************************************************************************
// code imported from Intel's gtpin sample
//*****************************************************************************

/*========================== begin_copyright_notice ============================
Copyright (C) 2016-2022 Intel Corporation

SPDX-License-Identifier: MIT
============================= end_copyright_notice ===========================*/

bool
Use64BitCounters
(const IGtGenCoder &coder)
{
  return coder.InstructionFactory().CanAccessAtomically(GED_DATA_TYPE_uq);
}


void
KernelProfile::GeneratePreCode
(GtGenProcedure &proc,
 const IGtGenCoder &coder)
{
  coder.StartTimer(proc, _timeReg);
  if (!proc.empty()) {
    proc.front()->AppendAnnotation(__func__);
  }
}


bool
KernelProfile::GeneratePostCodeRegBound
(GtGenProcedure &postProc,
 GtGenProcedure &finiProc,
 const IGtGenCoder &coder,
 const GtProfileArray &profileArray,
 uint32_t recordNum)
{
  IGtInsFactory &insF = coder.InstructionFactory();
  IGtVregFactory &vregs = coder.VregFactory();
  IGtRegAllocator &ra = coder.RegAllocator();
  bool is64BitCounter = Use64BitCounters(coder);

  GtReg flagReg = FlagReg(0);
  GtReg freqReg = vregs.MakeCounter(VREG_TYPE_DWORD);                                     // Frequency counter
  GtReg cycleReg = vregs.MakeCounter(is64BitCounter ? VREG_TYPE_QWORD : VREG_TYPE_DWORD); // Cycle counter
  GtReg cycleRegL = {cycleReg, sizeof(uint32_t), 0};                                      // Low 32-bits of cycle counter
  GtReg dataRegL = {_dataReg, sizeof(uint32_t), 0};                                       // Low 32-bits of '_dataReg'

  // Generate procedure that computes and aggregates cycles and frequency
  GtGenProcedure proc;
  coder.StopTimerExt(proc, _timeReg);
  proc += insF.MakeAdd(cycleRegL, cycleRegL, _timeReg); // Add elapsed time to lower 32-bits of of 'cycleReg'
  if (is64BitCounter) {
    // If cycleRegL overflowed, increment the high 32-bits of 'cycleReg'
    GtReg cycleRegH = {cycleReg, sizeof(uint32_t), 1};
    proc += insF.MakeCmp(GED_COND_MODIFIER_l, flagReg, cycleRegL, _timeReg);
    proc += insF.MakeAdd(cycleRegH, cycleRegH, 1).SetPredicate(flagReg);
  }
  proc += insF.MakeAdd(freqReg, freqReg, 1); // freq++

  if (!ra.ReserveVregOperands(proc)) {
    return false; // No more free registers
  }

  proc.front()->AppendAnnotation(__func__);
  postProc.MoveAfter(std::move(proc));

  // Generate 'finiProc' procedure that stores aggregated cycles and frequency counters in the profile buffer
  profileArray.ComputeAddress(coder, proc, _addrReg, recordNum);
  proc += insF.MakeRegMov(_dataReg, cycleReg);
  proc += insF.MakeAtomicAdd(NullReg(), _addrReg, _dataReg, (is64BitCounter ? GED_DATA_TYPE_uq : GED_DATA_TYPE_ud));

  profileArray.ComputeRelAddress(coder, proc, _addrReg, _addrReg, offsetof(LatencyRecord, freq));
  proc += insF.MakeRegMov(dataRegL, freqReg);
  proc += insF.MakeAtomicAdd(NullReg(), _addrReg, _dataReg, GED_DATA_TYPE_ud);

  proc.front()->AppendAnnotation("GenerateFiniCode");
  finiProc.MoveAfter(std::move(proc));

  return true;
}


void
KernelProfile::GeneratePostCodeMemBound
(GtGenProcedure &proc,
 const IGtGenCoder &coder,
 const GtProfileArray &profileArray,
 uint32_t recordNum)
{
  IGtInsFactory &insF = coder.InstructionFactory();
  bool is64BitCounter = Use64BitCounters(coder);
  GtReg dataRegL = {_dataReg, sizeof(uint32_t), 0}; // Low 32-bits of the data payload register

  // Generate code that computes elapsed time
  coder.StopTimerExt(proc, _timeReg);

  // _addrReg =  address of the current thread's LatencyRecord in the profile buffer
  profileArray.ComputeAddress(coder, proc, _addrReg, recordNum);

  // cycles += _timeReg
  proc += insF.MakeMov(dataRegL, _timeReg); // Move timer value to the low 32-bits of the data register
  if (is64BitCounter) {
    // Clear the high 32-bits of the data payload register
    GtReg dataRegH = {_dataReg, sizeof(uint32_t), 1};
    proc += insF.MakeMov(dataRegH, 0);
  }
  proc += insF.MakeAtomicAdd(NullReg(), _addrReg, _dataReg, (is64BitCounter ? GED_DATA_TYPE_uq : GED_DATA_TYPE_ud));

  // freq++
  profileArray.ComputeRelAddress(coder, proc, _addrReg, _addrReg, offsetof(LatencyRecord, freq));
  proc += insF.MakeAtomicInc(NullReg(), _addrReg, GED_DATA_TYPE_ud);

  if (!proc.empty()) {
    proc.front()->AppendAnnotation(__func__);
  }
}

//*****************************************************************************
// private operations
//*****************************************************************************

static void
calculate_instruction_latencies
(std::vector<Instruction> &instructions,
 uint64_t block_execution_count,
 uint64_t block_latency) {
  static int w_min = 4;
  static int c_weight = 2;

  static auto safe_division = [](float first, float second) {
    return second ? first / second : 0;
  };

  int unpredictable_instructions = 0;
  float sum_predictable_latency = 0;
  float average_latency = safe_division(block_latency, block_execution_count);

  for (Instruction &instruction : instructions) {
    int size = instruction.execSize <= 4 ? 4 : instruction.execSize;
    instruction.wIns = size * w_min / 4;
    if (instruction.isPredictable) {
      if (instruction.isComplex) {
	instruction.wIns *= c_weight;
      }
      sum_predictable_latency += instruction.wIns;
    } else {
      ++unpredictable_instructions;
    }
  }

  float w_unpredictable_instructions =
    safe_division(average_latency - sum_predictable_latency, unpredictable_instructions), sum_instruction_weights = 0;

  for (Instruction &instruction : instructions) {
    if (!instruction.isPredictable && w_unpredictable_instructions > 0) {
      instruction.wIns = w_unpredictable_instructions;
    }
    sum_instruction_weights += instruction.wIns;
  }

  float normalization_factor = safe_division(average_latency, sum_instruction_weights);
  for (Instruction &instruction : instructions) {
    instruction.wIns *= normalization_factor;
    instruction.latency = round(block_execution_count * instruction.wIns);
  }
}


static int find_or_add_loadmap_module
(const IGtKernel &kernel) {
  auto it = hash_map.find(kernel.Id());
  if (it) {
    return *it;
  }

  auto kernel_name_temp = (static_cast<std::string>(kernel.Name()));
  auto kernel_name = kernel_name_temp.c_str();

  char *kernel_elf = (char *)kernel.Elf().data();
  uint32_t kernel_elf_size = kernel.Elf().size();

  if (kernel_elf_size == 0) return -1;

  char file_name[CRYPTO_HASH_STRING_LENGTH];
  memset(file_name, 0, CRYPTO_HASH_STRING_LENGTH);
  gtpin_hpcrun_api->crypto_compute_hash_string(kernel_elf, kernel_elf_size, file_name, sizeof(file_name));

  char kernel_name_hash[PATH_MAX];
  gtpin_hpcrun_api->crypto_compute_hash_string(kernel_name, strlen(kernel_name), kernel_name_hash, sizeof(kernel_name_hash));

  char path[PATH_MAX];
  memset(path, 0, PATH_MAX);

  char *thename = kernel_name_hash;

  gtpin_hpcrun_api->gpu_binary_path_generate(thename, path);

  gtpin_hpcrun_api->gpu_binary_store(path, kernel_elf, kernel_elf_size);

  if (!gtpin_modern) {
    strcat(path, ".");
    strncat(path, kernel_name_hash, strlen(kernel_name_hash));
  }

  auto hash = gtpin_hpcrun_api->gpu_binary_loadmap_insert(path, true);
  hash_map.try_emplace(kernel.Id(), hash);

  return hash;
}


static void
create_igt_kernel_node
(GtKernelExecDesc exec_desc, GPU_PLATFORM platform)
{
  CorrelationData *correlation_data = new CorrelationData();
  if (correlation_tail == NULL)
    {
      correlation_head.next = NULL;
      correlation_tail = &correlation_head;
    }

  correlation_data->correlation_id = exec_desc.ToString(platform);

  correlation_data->cpu_submit_time = gtpin_hpcrun_api->hpcrun_nanotime();
  correlation_data->next = NULL;
  correlation_tail->next = correlation_data;
  correlation_tail = correlation_data;
}


static void
process_block_activity
(uint32_t loadmapModuleId,
 CorrelationData &correlation_data,
 BasicBlock &basicBlock)
{
  gpu_activity_channel_t *activity_channel = correlation_data.activity_channel;
  gpu_op_ccts_t *op_ccts = &correlation_data.op_ccts;

  gtpin_hpcrun_api->safe_enter();

  cct_node_t *host_op_node = gtpin_hpcrun_api->gpu_op_ccts_get(op_ccts, gpu_placeholder_type_kernel);

  gpu_activity_t ga;
  memset(&ga.details.kernel_block, 0, sizeof(gpu_kernel_block_t));

  ga.details.kernel_block.pc.lm_id = (uint16_t)loadmapModuleId;
  ga.details.kernel_block.pc.lm_ip = (uintptr_t)basicBlock.instructions[0].offset;
  ga.details.kernel_block.execution_count = basicBlock.executionCount;
  ga.details.kernel_block.latency = basicBlock.latency;
  ga.details.kernel_block.active_simd_lanes = basicBlock.activeSimdLanes;
  ga.kind = GPU_ACTIVITY_KERNEL_BLOCK;
  gtpin_hpcrun_api->cstack_ptr_set(&(ga.next), 0);

  cct_node_t *cct_node = gtpin_hpcrun_api->hpcrun_cct_insert_ip_norm(host_op_node, ga.details.kernel_block.pc, false);
  if (cct_node) {
    ga.cct_node = cct_node;
    gtpin_hpcrun_api->gpu_operation_multiplexer_push(activity_channel, NULL, &ga);
  }

  gtpin_hpcrun_api->safe_exit();
}


static void
visit_block
(cct_node_t *block,
 cct_op_arg_t arg,
 size_t level) {
  uint16_t lm_id;
  uintptr_t lm_ip;
  gtpin_hpcrun_api->get_cct_node_id(block, &lm_id, &lm_ip);

  BasicBlock basicBlock = block_map[std::make_pair(lm_id, lm_ip)];

  if (basicBlock.instructions.size()) {
    std::vector<std::pair<cct_node_t *, BasicBlock>> *data_vector = (std::vector<std::pair<cct_node_t *, BasicBlock>> *)arg;
    (*data_vector).push_back(std::make_pair(block, basicBlock));
  }
}



//*****************************************************************************
// class implementations
//*****************************************************************************

BasicBlock::BasicBlock
(const IGtCfg &cfg,
 const IGtBbl &bbl,
 uint32_t simdWidthIn,
 bool simd = false) :
  executionCount(0),
  latency(0),
  simdWidth(simdWidthIn),
  activeSimdLanes(0) {
  std::set<SimdGroup> simdMap;

  for (const IGtIns *ins : bbl.Instructions()) {
    Instruction instruction = Instruction(*ins);
    instruction.offset = cfg.GetInstructionOffset(*ins);

    instructions.push_back(instruction);

    if (simd) {
      auto it = simdMap.emplace(SimdGroup(*ins)).first;
      SimdGroup &it2 = const_cast<SimdGroup &>(*it);
      it2.instructions.push_back(instruction);

      if (ins->IsFlagModifier() || (ins->Id() == bbl.LastIns().Id())) {
	for (auto &item : simdMap) {
	  simdGroups.push_back(item);
	}
	simdMap.clear();
      }
    }
  }
}


Instruction::Instruction
(const IGtIns &ins) {
  uint32_t execSize = ins.ExecSize();
  GED_OPCODE opcode = ins.Opcode();
  this->execSize = execSize;
  isPredictable = opcode < GED_OPCODE_send && opcode > GED_OPCODE_sendsc;
  isComplex = opcode == GED_OPCODE_math;
  latency = 0;
}


void
KernelProfile::instrument
(IGtKernelInstrument &instrument) {
  const IGtCfg &cfg = instrument.Cfg();
  const IGtKernel &kernel = instrument.Kernel();
  const IGtGenCoder &coder = instrument.Coder();
  IGtProfileBufferAllocator &allocator = instrument.ProfileBufferAllocator();
  IGtVregFactory &vregs = coder.VregFactory();
  IGtInsFactory &insF = coder.InstructionFactory();
  bool is64BitCounter = insF.CanAccessAtomically(GED_DATA_TYPE_uq);
  IGtRegAllocator &ra = coder.RegAllocator();

  _timeReg = vregs.Make(VREG_TYPE_DWORD);
  _addrReg = vregs.MakeMsgAddrScratch();
  _dataReg = vregs.MakeMsgDataScratch(is64BitCounter ? VREG_TYPE_QWORD : VREG_TYPE_DWORD);

  ra.Reserve(_dataReg.VregNumber());
  ra.Reserve(_addrReg.VregNumber());
  ra.Reserve(vregs.GetProfileBufferAddrVreg().Num());

  if (count_knob) {
    opcodeProfile = GtProfileArray(sizeof(OpcodeRecord), cfg.NumBbls(), kernel.GenModel().MaxThreadBuckets());
    opcodeProfile.Allocate(allocator);

    for (const IGtBbl *bbl : cfg.Bbls()) {
      BasicBlock basicBlock = BasicBlock(cfg, *bbl, instrument.Kernel().SimdWidth(), simd_knob);
      basicBlocks.push_back(basicBlock);
      simdGroupsCount += basicBlock.simdGroups.size();

      block_map[std::make_pair(loadmapModuleId, basicBlock.instructions[0].offset)] = basicBlock;

      GtGenProcedure proc;
      opcodeProfile.ComputeAddress(coder, proc, _addrReg, bbl->Id());
      proc += insF.MakeAtomicInc(NullReg(), _addrReg, GED_DATA_TYPE_ud);
      if (!proc.empty()) {
	proc.front()->AppendAnnotation(__func__);
      }
      instrument.InstrumentBbl(*bbl, GtIpoint::Before(), proc);
    }
  }

  if (latency_knob) {
    std::vector<const IGtBbl *> bbls;
    for (auto bblPtr : cfg.Bbls()) {
      if (!bblPtr->IsEmpty() && !bblPtr->FirstIns().IsChangingIP()) {
	bbls.push_back(bblPtr);
      }
    }

    latencyProfile = GtProfileArray(sizeof(LatencyRecord), bbls.size(), kernel.GenModel().MaxThreadBuckets());
    latencyProfile.Allocate(allocator);

    GtGenProcedure finiCode;
    bool tryRegInstrument = true;
    int index = 0;


    for (const IGtBbl *bbl : bbls) {
      BasicBlock basicBlock = BasicBlock(cfg, *bbl, instrument.Kernel().SimdWidth(), simd_knob);
      basicBlocks.push_back(basicBlock);
      simdGroupsCount += basicBlock.simdGroups.size();

      block_map[std::make_pair(loadmapModuleId, basicBlock.instructions[0].offset)] = basicBlock;

      GtGenProcedure preCode;
      GeneratePreCode(preCode, coder);
      instrument.InstrumentBbl(*bbl, GtIpoint::Before(), preCode);

      GtGenProcedure postCode;
      if (bbl->IsEot()) {
	const IGtIns &eotIns = bbl->LastIns();
	coder.GenerateFakeSrcConsumers(postCode, eotIns);
	GeneratePostCodeMemBound(postCode, coder, latencyProfile, index);
	instrument.InstrumentInstruction(eotIns, GtIpoint::Before(), postCode);
      } else {
	if (tryRegInstrument) {
	  tryRegInstrument = GeneratePostCodeRegBound(postCode, finiCode, coder, latencyProfile, index);
	}
	if (!tryRegInstrument) {
	  GeneratePostCodeMemBound(postCode, coder, latencyProfile, index);
	}
	instrument.InstrumentBbl(*bbl, GtIpoint::After(), postCode);                
                
	// GeneratePostCodeMemBound(postCode, coder, latencyProfile, index);
	// instrument.InstrumentBbl(*bbl, GtIpoint::After(), postCode);
      }
      ++index;
    }

    instrument.InstrumentExits(finiCode);
  }

  ra.ReleaseReserved(_dataReg.VregNumber());
  ra.ReleaseReserved(_addrReg.VregNumber());
  ra.ReleaseReserved(vregs.GetProfileBufferAddrVreg().Num());

  if (simd_knob) {
    GtReg dataRegL = {_dataReg, sizeof(uint32_t), 0};

    simdProfile = GtProfileArray(sizeof(SimdRecord), simdGroupsCount, kernel.GenModel().MaxThreadBuckets());
    simdProfile.Allocate(allocator);

    uint32_t index = 0;

    for (const BasicBlock &bbl : basicBlocks) {
      for (const SimdGroup &simdGroup : bbl.simdGroups) {
	GtGenProcedure proc;
	coder.ComputeSimdMask(proc, dataRegL, simdGroup.maskCtrl, simdGroup.execMask, simdGroup.predicate);
	if (!simdGroup.isSendIns) {
	  proc += insF.MakeCbit(dataRegL, dataRegL);
	} else {
	  proc += insF.MakeSel(dataRegL, dataRegL, 1).SetCondModifier(GED_COND_MODIFIER_l);
	}

	simdProfile.ComputeAddress(coder, proc, _addrReg, index++);

	if (is64BitCounter) {
	  GtReg dataRegH = {_dataReg, sizeof(uint32_t), 1};
	  proc += insF.MakeMov(dataRegH, 0);
	}
	proc += insF.MakeAtomicAdd(NullReg(), _addrReg, _dataReg, (is64BitCounter ? GED_DATA_TYPE_uq : GED_DATA_TYPE_ud));

	proc.front()->AppendAnnotation(__func__);
	instrument.InstrumentInstruction(cfg.GetInstruction(simdGroup.sectionHeadId), GtIpoint::Before(), proc);
      }
    }
  }

}


void
KernelProfile::gatherMetrics
(IGtKernelDispatch &dispatch,
 CorrelationData correlation_data)
{
  const IGtProfileBuffer *buffer = dispatch.GetProfileBuffer();
  GTPIN_ASSERT(buffer);

  uint32_t index = 0;

  for (BasicBlock &bbl : basicBlocks) {
    for (uint32_t threadBucket = 0; threadBucket < dispatch.Kernel().GenModel().MaxThreadBuckets(); ++threadBucket) {
      if (count_knob) {
	OpcodeRecord record;
	if (opcodeProfile.Read(*buffer, &record, index, 1, threadBucket)) {
	  bbl.executionCount += record.freq;
	}
      }

      if (latency_knob) {
	LatencyRecord record;
	if (latencyProfile.Read(*buffer, &record, index, 1, threadBucket)) {
	  bbl.executionCount += record.freq;
	  if (collect_latency) {
	    bbl.latency += record.cycles;
	  }
	}
      }

      if (simd_knob) {
	uint32_t simd_index = 0;

	for (SimdGroup &simdGroup : bbl.simdGroups) {
	  SimdRecord record;
	  if (simdProfile.Read(*buffer, &record, simd_index, 1, threadBucket)) {
	    bbl.activeSimdLanes += record.opCount * simdGroup.instructions.size();
	  }
	  ++simd_index;
	}
      }
    }
    // std::cout << "BLOCK WITH ID: " << index << " HAS LATENCY: " << bbl.latency << std::endl;

    ++index;
    process_block_activity(loadmapModuleId, correlation_data, bbl);

  }
}


void
GTPinInstrumentation::OnKernelBuild
(IGtKernelInstrument &instrument) {
  int id = find_or_add_loadmap_module(instrument.Kernel());
  if (id >= 0) {
    KernelProfile kernelProfile = KernelProfile(id);
    kernelProfile.instrument(instrument);
    _kernels.try_emplace(instrument.Kernel().Id(), kernelProfile);
  }
}


void
GTPinInstrumentation::OnKernelRun
(IGtKernelDispatch &dispatch) {
  GtKernelExecDesc execDesc;
  dispatch.GetExecDescriptor(execDesc);
  create_igt_kernel_node(execDesc, dispatch.Kernel().GpuPlatform());

  auto kernelProfile = _kernels.find(dispatch.Kernel().Id());
  if (kernelProfile) {
    IGtProfileBuffer *buffer = dispatch.CreateProfileBuffer();

    if ((count_knob && kernelProfile->opcodeProfile.Initialize(*buffer)) ||
	(latency_knob && kernelProfile->latencyProfile.Initialize(*buffer)) ||
	(simd_knob && kernelProfile->simdProfile.Initialize(*buffer))) {
      dispatch.SetProfilingMode(true);
    }
  }
}


bool
GTPinInstrumentation::Register
(void) {
  IGtCore *core = GTPin_GetCore();

  gtpin_modern =  strcmp(core->Version(), "3.0") > 0;

  if (!core->RegisterTool(*this)) {
    GTPIN_ERROR_MSG(std::string(Name()) + ": Failed registration with GTPin core. " +
		    "GTPin API version = " + std::to_string(core->ApiVersion()) +
		    "Tool API version = " + std::to_string(ApiVersion()));
    return false;
  }
  return true;
}


void
GTPinInstrumentation::OnKernelComplete
(IGtKernelDispatch &dispatch) {
  if (!dispatch.IsProfilingEnabled()) {
    return;
  }
  gtpin_hpcrun_api->hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);

  GtKernelExecDesc exec_desc;
  dispatch.GetExecDescriptor(exec_desc);
  CorrelationData correlation_data = correlation_map[exec_desc.ToString(dispatch.Kernel().GpuPlatform())];

  auto it = _kernels.find(dispatch.Kernel().Id());
  if (it) {
    it->gatherMetrics(dispatch, correlation_data);
  }
}



//*****************************************************************************
// interface to Intel's gtpin C++ library
//*****************************************************************************

extern "C" {
  IGtCore *GTPin_GetCore
  (void) {
    static IGtCore *core = NULL;

    if (core == NULL) {
      void *handler = dlmopen(LM_ID_BASE, "libgtpin.so", RTLD_LOCAL | RTLD_LAZY);
      if (handler == NULL) {
	EEMSG("FATAL: hpcrun failure: unable to load Intel's gtpin library: %s", dlerror());
	monitor_real_exit(-1);
      }

      IGtCore *(*getcore)() = (IGtCore * (*)()) dlsym(handler, "GTPin_GetCore");
      if (getcore == NULL) {
	EEMSG("FATAL: hpcrun failure: failed to bind function in Intel's gtpin library: %s", dlerror());
	monitor_real_exit(-1);
      }
      core = getcore();
    }

    return core;
  }
}



//*****************************************************************************
// interface operations
//*****************************************************************************

void
init_gtpin_hpcrun_api
(gtpin_hpcrun_api_t *functions) {
  gtpin_hpcrun_api = functions;
}


void
gtpin_instrumentation_options
(gpu_instrumentation_t *instrumentation) {
  latency_knob = instrumentation->attribute_latency;
  collect_latency = instrumentation->attribute_latency;
  simd_knob = instrumentation->analyze_simd;
#if 0
  // this is the way things should work
  count_knob = !latency_knob && (instrumentation->count_instructions || simd_knob);
#else
  // instruction counting alone currently destroys correctness.
  // for now, replace instruction counting with latency measurements, which
  // performs instruction counting as a side effect.
  if (instrumentation->count_instructions) {
    latency_knob = true;
  }
  count_knob = false;
#endif

  if (count_knob || latency_knob || simd_knob) {
    // customize GTPin behaviors
    if (instrumentation->silent) {
      SetKnobValue<bool>(true, "silent_warnings");       // don't print GTPin warnings
    }
    SetKnobValue<bool>(true, "no_empty_profile_dir");    // don't create GTPin profile directory
    // SetKnobValue<bool>(true, "xyzzy");                   // enable developer options
    // SetKnobValue<bool>(true, "prefer_lsc_scratch");      // use LSC scratch messages in PVC+

    // register our GTPin instance
    GTPinInstrumentation::Instance()->Register();
  }
}


void
gtpin_produce_runtime_callstack
(gpu_op_ccts_t *op_ccts) {
  assert(correlation_head.next != NULL);
  CorrelationData *correlation_data = correlation_head.next;

  correlation_map[correlation_data->correlation_id] = {
    .correlation_id = correlation_data->correlation_id,
    .cpu_submit_time = correlation_data->cpu_submit_time,
    .op_ccts = *op_ccts,
    .activity_channel = gtpin_hpcrun_api->gpu_activity_channel_get(),
    .next = NULL};

  if (correlation_data->next == NULL) {
    correlation_tail = &correlation_head;
  }
  correlation_head.next = correlation_data->next;
  delete correlation_data;
}


void
process_block_instructions
(cct_node_t *root) {
  std::vector<std::pair<cct_node_t *, BasicBlock>> data_vector;
  gtpin_hpcrun_api->hpcrun_cct_walk_node_1st(root, visit_block, &data_vector);

  for (const auto &data : data_vector) {
    cct_node_t *block = data.first;
    BasicBlock basicBlock = data.second;
    block_metrics_t block_metrics = gtpin_hpcrun_api->fetch_block_metrics(block);
    uint64_t executionCount = count_knob || latency_knob ? block_metrics.execution_count : 0;
    uint64_t latency = latency_knob ? block_metrics.latency : 0;
    uint64_t totalSimdLanes = simd_knob ? block_metrics.execution_count * basicBlock.simdWidth * basicBlock.instructions.size() : 0;
    uint64_t activeSimdLanes = block_metrics.active_simd_lanes < totalSimdLanes ? block_metrics.active_simd_lanes : totalSimdLanes;

    if (latency) {
      calculate_instruction_latencies(basicBlock.instructions, block_metrics.execution_count, block_metrics.latency);
    }

    for (const Instruction &instruction : basicBlock.instructions) {
      uint64_t scalarSimdLoss = simd_knob && instruction.execSize == 1 ? basicBlock.simdWidth - 1 : 0;
      cct_node_t *child = gtpin_hpcrun_api->hpcrun_cct_insert_instruction_child(block, instruction.offset);
      gtpin_hpcrun_api->attribute_instruction_metrics
	(child,
	 {.execution_count = executionCount,
	     .latency = instruction.latency,
	     .total_simd_lanes = totalSimdLanes,
	     .active_simd_lanes = activeSimdLanes,
	     .wasted_simd_lanes = totalSimdLanes - activeSimdLanes,
	     .scalar_simd_loss = scalarSimdLoss});
    }
  }
}
