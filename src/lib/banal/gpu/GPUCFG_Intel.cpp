// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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


#ifdef ENABLE_IGC

//******************************************************************************
// system includes
//******************************************************************************

#include <fcntl.h>
#include <fstream>                  // ofstream
#include <iostream>
#include <libelf.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <Graph.h>                  // Graph
#include <slicing.h>                // Slicer
#include <Symtab.h>
#include <CodeSource.h>
#include <CodeObject.h>



//***************************************************************************
// Intel IGA
//***************************************************************************

#include <iga/kv.hpp>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/binutils/ElfHelper.hpp>
#include <lib/support/diagnostics.h>

#include "GPUCFG.hpp"
#include "GPUCFGFactory.hpp"
#include "GPUFunction.hpp"
#include "GPUBlock.hpp"
#include "GPUCodeSource.hpp"
#include "GPUCFG_Intel.hpp"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG_CFG                  0
#define DEBUG_INSTRUCTION_STAT     0
#define DEBUG_INSTRUCTION_ANALYZER 0

#define MAX_STR_SIZE 1024
#define INTEL_GPU_DEBUG_SECTION_NAME "Intel(R) OpenCL Device Debug"



//******************************************************************************
// type declarations
//******************************************************************************

class IntelGPUInstDumper : public GPUParse::GPUInstDumper {
public:
  IntelGPUInstDumper(GPUParse::Function &_function, KernelView &_kv) :
    GPUInstDumper(_function), kv(_kv) {}
  void dump(GPUParse::Inst* inst);
private:
  KernelView &kv;
};

using TargetType = Dyninst::ParseAPI::EdgeTypeEnum;



//******************************************************************************
// local variables
//******************************************************************************

static int TRACK_LIMIT = 8;



//******************************************************************************
// namespace imports
//******************************************************************************

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;



//******************************************************************************
// private functions
//******************************************************************************

static std::string
getOpString(iga::Op op) {
  switch (op) {
    case iga::Op::ADD:      return "ADD";
    case iga::Op::ADDC:     return "ADDC";
    case iga::Op::AND:      return "AND";
    case iga::Op::ASR:      return "ASR";
    case iga::Op::AVG:      return "AVG";
    case iga::Op::BFE:      return "BFE";
    case iga::Op::BFI1:     return "BFI1";
    case iga::Op::BFI2:     return "BFI2";
    case iga::Op::BFREV:    return "BFREV";
    case iga::Op::BRC:      return "BRC";
    case iga::Op::BRD:      return "BRD";
    case iga::Op::BREAK:    return "BREAK";
    case iga::Op::CALL:     return "CALL";
    case iga::Op::CALLA:    return "CALLA";
    case iga::Op::CBIT:     return "CBIT";
    case iga::Op::CMP:      return "CMP";
    case iga::Op::CMPN:     return "CMPN";
    case iga::Op::CONT:     return "CONT";
    case iga::Op::CSEL:     return "CSEL";
    case iga::Op::DIM:      return "DIM";
    case iga::Op::DP2:      return "DP2";
    case iga::Op::DP3:      return "DP3";
    case iga::Op::DP4:      return "DP4";
    case iga::Op::DP4A:     return "DP4A";
    case iga::Op::DPH:      return "DPH";
    case iga::Op::ELSE:     return "ELSE";
    case iga::Op::ENDIF:    return "ENDIF";
    case iga::Op::F16TO32:  return "F16TO32";
    case iga::Op::F32TO16:  return "F32TO16";
    case iga::Op::FBH:      return "FBH";
    case iga::Op::FBL:      return "FBL";
    case iga::Op::FRC:      return "FRC";
    case iga::Op::GOTO:     return "GOTO";
    case iga::Op::HALT:     return "HALT";
    case iga::Op::IF:       return "IF";
    case iga::Op::ILLEGAL:  return "ILLEGAL";
    case iga::Op::JMPI:     return "JMPI";
    case iga::Op::JOIN:     return "JOIN";
    case iga::Op::LINE:     return "LINE";
    case iga::Op::LRP:      return "LRP";
    case iga::Op::LZD:      return "LZD";
    case iga::Op::MAC:      return "MAC";
    case iga::Op::MACH:     return "MACH";
    case iga::Op::MAD:      return "MAD";
    case iga::Op::MADM:     return "MADM";
    case iga::Op::MATH:     return "MATH";
    case iga::Op::MOV:      return "MOV";
    case iga::Op::MOVI:     return "MOVI";
    case iga::Op::MUL:      return "MUL";
    case iga::Op::NOP:      return "NOP";
    case iga::Op::NOT:      return "NOT";
    case iga::Op::OR:       return "OR";
    case iga::Op::PLN:      return "PLN";
    case iga::Op::RET:      return "RET";
    case iga::Op::RNDD:     return "RNDD";
    case iga::Op::RNDE:     return "RNDE";
    case iga::Op::RNDU:     return "RNDU";
    case iga::Op::RNDZ:     return "RNDZ";
    case iga::Op::ROL:      return "ROL";
    case iga::Op::ROR:      return "ROR";
    case iga::Op::SAD2:     return "SAD2";
    case iga::Op::SADA2:    return "SADA2";
    case iga::Op::SEL:      return "SEL";
    case iga::Op::SEND:     return "SEND";
    case iga::Op::SENDC:    return "SENDC";
    case iga::Op::SENDS:    return "SENDS";
    case iga::Op::SENDSC:   return "SENDSC";
    case iga::Op::SHL:      return "SHL";
    case iga::Op::SHR:      return "SHR";
    case iga::Op::SMOV:     return "SMOV";
    case iga::Op::SUBB:     return "SUBB";
    case iga::Op::SYNC:     return "SYNC";
    case iga::Op::WAIT:     return "WAIT";
    case iga::Op::WHILE:    return "WHILE";
    case iga::Op::XOR:      return "XOR";
    default:                return "INVALID";
  }
}


#if DEBUG_INSTRUCTION_STAT

static std::string getKindString(iga::Kind kind) {
  switch (kind)
  {
    case iga::Kind::INVALID:    return "INVALID";   // an invalid or uninitialized operand
    case iga::Kind::DIRECT:     return "DIRECT";    // direct register reference
    case iga::Kind::MACRO:      return "MACRO";     // madm or math.invm or math.rsqrtm
    case iga::Kind::INDIRECT:   return "INDIRECT";  // register-indriect access
    case iga::Kind::IMMEDIATE:  return "IMMEDIATE"; // immediate value
    case iga::Kind::LABEL:      return "LABEL";     // block target (can be numeric label/i.e. imm value)
    default:                    return "UNKNOWN";
  }
}


static std::string getRegNameString(iga::RegName reg)
{
  switch (reg) {
    case iga::RegName::GRF_R:       return  "GRF";
    case iga::RegName::ARF_NULL:    return  "AREG_NULL";
    case iga::RegName::ARF_A:       return  "AREG_A";
    case iga::RegName::ARF_ACC:     return  "AREG_ACC";
    case iga::RegName::ARF_CE:      return  "AREG_MASK0";
    case iga::RegName::ARF_MSG:     return  "AREG_MSG";
    case iga::RegName::ARF_DBG:     return  "AREG_DBG";
    case iga::RegName::ARF_SR:      return  "AREG_SR0";
    case iga::RegName::ARF_CR:      return  "AREG_CR0";
    case iga::RegName::ARF_N:       return  "AREG_N";
    case iga::RegName::ARF_IP:      return  "AREG_IP";
    case iga::RegName::ARF_F:       return  "AREG_F";
    case iga::RegName::ARF_TM:      return  "AREG_TM0";
    case iga::RegName::ARF_TDR:     return  "AREG_TDR0";
    case iga::RegName::ARF_SP:      return  "AREG_SP";
    case iga::RegName::ARF_MME:     return  "AREG_MME";
    case iga::RegName::ARF_FC:      return  "AREG_FC";
    default:                        return  "REG_INVALID";
  }
}


static std::string getIGATypeString(iga::Type type)
{
    switch (type) {
    case iga::Type::UB:   return "Type_UB";
    case iga::Type::B:    return "Type_B";
    case iga::Type::UW:   return "Type_UW";
    case iga::Type::W:    return "Type_W";
    case iga::Type::UD:   return "Type_UD";
    case iga::Type::D:    return "Type_D";
    case iga::Type::UQ:   return "Type_UQ";
    case iga::Type::Q:    return "Type_Q";
    case iga::Type::HF:   return "Type_HF";
    case iga::Type::F:    return "Type_F";
    case iga::Type::DF:   return "Type_DF";
    case iga::Type::UV:   return "Type_UV";
    case iga::Type::V:    return "Type_V";
    case iga::Type::VF:   return "Type_VF";
    case iga::Type::NF:   return "Type_NF";
    default:              return "Type_INVALID";
    }
}


static std::string getIGAPredCtrlString(iga::PredCtrl predCtrl)
{
    switch (predCtrl) {
      case iga::PredCtrl::SEQ:        return "PRED_DEFAULT";
      case iga::PredCtrl::ANY2H:      return "PRED_ANY2H";
      case iga::PredCtrl::ANY4H:      return "PRED_ANY4H";
      case iga::PredCtrl::ANY8H:      return "PRED_ANY8H";
      case iga::PredCtrl::ANY16H:     return "PRED_ANY16H";
      case iga::PredCtrl::ANY32H:     return "PRED_ANY32H";
      case iga::PredCtrl::ALL2H:      return "PRED_ALL2H";
      case iga::PredCtrl::ALL4H:      return "PRED_ALL4H";
      case iga::PredCtrl::ALL8H:      return "PRED_ALL8H";
      case iga::PredCtrl::ALL16H:     return "PRED_ALL16H";
      case iga::PredCtrl::ALL32H:     return "PRED_ALL32H";
      case iga::PredCtrl::ANYV:       return "PRED_ANYV";
      case iga::PredCtrl::ALLV:       return "PRED_ALLV";
      default:                        return "PRED_NONE";
    }
}

#endif


static int
getElementSize
(
 iga::Type dataType
)
{
  // values for NF and INVALID not added
  if (dataType == iga::Type::UB || dataType == iga::Type::B) {
    return 1;
  } else if (dataType == iga::Type::UW || dataType == iga::Type::W) {
    return 2;
  } else if (dataType == iga::Type::UD || dataType == iga::Type::D || dataType == iga::Type::UV ||
      dataType == iga::Type::V || dataType == iga::Type::VF || dataType == iga::Type::F) {
    return 4;
  } else if (dataType == iga::Type::UQ || dataType == iga::Type::Q || dataType == iga::Type::DF) {
    return 8;
  } else {
    return 0;
  }
}


static void
addCustomFunctionObject
(
 const std::string &func_obj_name,
 Symtab *symtab
)
{
  Region *reg = NULL;
  bool status = symtab->findRegion(reg, ".text");
  assert(status == true);

  unsigned long reg_size = reg->getMemSize();
  Symbol *custom_symbol = new Symbol(
      func_obj_name,
      SymtabAPI::Symbol::ST_FUNCTION, // SymbolType
      Symbol::SL_LOCAL, //SymbolLinkage
      SymtabAPI::Symbol::SV_DEFAULT, //SymbolVisibility
      0, //Offset,
      NULL, //Module *module
      reg, //Region *r
      reg_size, //unsigned s
      false, //bool d
      false, //bool a
      -1, //int index
      -1, //int strindex
      false //bool cs
  );

  //adding the custom symbol into the symtab object
  status = symtab->addSymbol(custom_symbol); //(Symbol *newsym)
  assert(status == true);

  // After injecting symbol, we can parse inlining info
  symtab->parseTypesNow();
}


static GPUParse::InstructionStat*
getIntelInstructionStat
(
 const KernelView &kv,
 int offset
)
{
  char inst_asm_text[MAX_STR_SIZE] = { 0 };
  size_t length = kv.getInstSyntax(offset, inst_asm_text, MAX_STR_SIZE);
  assert(length > 0);
  iga::Op opcode = kv.getOpcode(offset);
  std::string op = getOpString(opcode);

  int execSize = (int)kv.getExecutionSize(offset); // returns iga::ExecSize
  int32_t noSrcReg = kv.getNumberOfSources(offset);

#if DEBUG_INSTRUCTION_STAT
  std::cout << "offset: " << offset << ". asm: " << inst_asm_text << std::endl;
  std::cout << "\n" "opcode:" << op << "\n";
  std::cout << "\n" "number of source registers: " << noSrcReg;
#endif

  std::vector<int> srcs;
  for (int i = 0;   i < noSrcReg; i++) {
    int32_t srcRegNo = kv.getSrcRegNumber(offset, i);
    int32_t srcSubRegNo = kv.getSrcSubRegNumber(offset, i);
    iga::Type srcDataType = kv.getSrcDataType(offset, i);
    iga::RegName srcRegType = kv.getSrcRegType(offset, i);

    if (srcRegType != iga::RegName::GRF_R) {
      continue;
    }
    uint32_t vertStride, width, horzStride;
    // Returns 0 if any of instruction's src operand region components
    // (Src RgnVt, RgnWi, RgnHz) are successfully determined.
    // Otherwise returns -1.
    int32_t status = kv.getSrcRegion(offset, i, &vertStride, &width, &horzStride);
    if (status != 0) {
      continue;
    }

#if DEBUG_INSTRUCTION_STAT
    iga::Kind srcRegKind = kv.getSrcRegKind(offset, i);
    std::cout << "\nSrcreg no: " << i << "\n  register: " << srcRegNo << ", subregister: " << srcSubRegNo
      << ", srcDataType: " << getIGATypeString(srcDataType)
      << ", srcRegType: " << getRegNameString(srcRegType)
      << ", srcRegKind: " << getKindString(srcRegKind)
      << ", execSize: " << execSize
      << "\n  stride fetch status: " << status << ", vert.stride: " << vertStride << ", width: "  << width << ", hor.stride: " << horzStride << std::endl;
#endif

    int elementSize = getElementSize(srcDataType);
    int height = execSize / width;
    int channel = 0;
    int base1 = (srcRegNo << 5) + srcSubRegNo * elementSize;
    std::vector<int> childSrc(execSize * elementSize);
    for (int x=0; x < height; x++) {
      int base2 = base1;
      for (uint32_t y=0; y < width; y++) {
        int addr_y = base2;
        for (int z=0;z<elementSize;z++) {
          childSrc[channel++] = addr_y + z;
        }
        base2 += horzStride*elementSize;
      }
      base1 +=vertStride*elementSize;
    }
    srcs.insert(srcs.end(), childSrc.begin(), childSrc.end());
  }

  int32_t dstRegNo = kv.getDstRegNumber(offset);
  int32_t dstSubRegNo = kv.getDstSubRegNumber(offset);
  iga::Type dstDataType = kv.getDstDataType(offset);
  iga::RegName dstRegType = kv.getDstRegType(offset);
  std::vector<int> dsts;
  if (dstRegType == iga::RegName::GRF_R) {
    uint32_t horzStride;
    // Returns 0 if instruction's destination operand horizontal stride
    // (DstRgnHz) is successfully returned.
    // Otherwise returns -1.
    int32_t status = kv.getDstRegion(offset, &horzStride);
    if (status == 0) {
      int elementSize = getElementSize(dstDataType);

      dsts.resize(execSize * elementSize);
      int channel = 0;
      int base1 = (dstRegNo << 5) + dstSubRegNo * elementSize;
      for (int x=0; x < execSize; x++) {
        int addr_x = base1;
        for (int y=0;y<elementSize;y++) {
          dsts[channel++] = addr_x + y;
        }
        base1 += (horzStride * elementSize);
      }

#if DEBUG_INSTRUCTION_STAT
      iga::Kind dstRegKind = kv.getDstRegKind(offset);
      std::cout << "\ndstRegNo: " << dstRegNo << ", subregister: " << dstSubRegNo
        << ", DataType: " << getIGATypeString(dstDataType)
        << ", RegType: " << getRegNameString(dstRegType)
        << ", RegKind: " << getKindString(dstRegKind)
        << ", execSize: " << execSize << std::endl;
#endif
    }
  } else {
    // To be considered: How to deal with writes to ARF registers?
  }

  // barriers are executed using send instruction. example:
  // [324] (W)      send (1|M0)              null     r22     0x3         0x2000004  //  wr:1+?, rd:0,  barrier
  // one can only have memory coherency and synchronization inside a 'work group' or 'thread group', depending on the nomenclature.
  // This instruction makes this thread wait until all threads in its group have entered the barrier.
  // AFAIK, all threads in a work-group share a single barrier
  // unlike CUDA, there is a single barrier register in intel instructions.
  // So is fine to send a vector of 1 entry when an instruction is a barrier and
  // empty vector when the instruction is not a barrier?

  // commenting this section since synchronization is not factored in backward slicing
#if 0
  bool instContainsBarrier = false;
  if (opcode == iga::Op::SEND || opcode == iga::Op::SENDC || opcode == iga::Op::SENDS || opcode == iga::Op::SENDSC) {
    char *output = strstr (inst_asm_text, "barrier");
    if (output) {
      instContainsBarrier = true;
    }
  }
#endif

  // intel instructions follow SIMD model.
  // when predication is on for an instruction, flag register is used to check which SIMD lanes should be used by the instruction
  // predication does not switch on/off an instruction, just some lanes
  // There are 3 more registers that affect lanes used (these registers affect all instructions, not just predicated inst.)
  // CE, ExecMask and DMask
  iga::PredCtrl pred = kv.getPredicate(offset);
  bool invPred = kv.isInversePredicate(offset);

#if FLAGREG
  int32_t flagReg = kv.getFlagReg(offset);
  int32_t flagSubReg = kv.getFlagSubReg(offset);
#endif

  GPUParse::InstructionStat::PredicateFlag predFlag;
  if (pred == iga::PredCtrl::NONE) {
    predFlag = GPUParse::InstructionStat::PredicateFlag::PREDICATE_NONE;
  } else if (invPred) {
    predFlag = GPUParse::InstructionStat::PredicateFlag::PREDICATE_FALSE;
  } else {
    predFlag = GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE;
  }

#if DEBUG_INSTRUCTION_STAT
  std::cout << "\npred: " << getIGAPredCtrlString(pred) << ",invPred: " << invPred
#if FLAGREG
    << ", flag register: " << flagReg << ", flag subregister: " << flagSubReg
#endif
  << std::endl;
#endif

  auto *inst_stat = new GPUParse::InstructionStat(op, offset, predFlag, dsts, srcs);
    //new InstructionStat(op, pc, pred, barrier_threshold, indirect, pred_flag, pred_assign_pcs, dsts, srcs,
    //    pdsts, psrcs, bdsts, bsrcs, udsts, usrcs, updsts, upsrcs, assign_pcs, passign_pcs,
    //    bassign_pcs, uassign_pcs, upassign_pcs, control);
  return inst_stat;
}


class FirstMatchPred : public Dyninst::Slicer::Predicates {
 public:
  virtual bool endAtPoint(Dyninst::Assignment::Ptr ap) { return true; }
};


class IgnoreRegPred : public Dyninst::Slicer::Predicates {
 public:
  IgnoreRegPred(std::vector<Dyninst::AbsRegion> &rhs) : _rhs(rhs) {}

  virtual bool modifyCurrentFrame(Dyninst::Slicer::SliceFrame &slice_frame,
                                  Dyninst::GraphPtr graph_ptr, Dyninst::Slicer *slicer) {
    std::vector<Dyninst::AbsRegion> delete_abs_regions;

    for (auto &active_iter : slice_frame.active) {
      // Filter unmatched regs
      auto &abs_region = active_iter.first;
      bool find = false;
      for (auto &rhs_abs_region : _rhs) {
        if (abs_region.absloc().reg() == rhs_abs_region.absloc().reg()) {
          find = true;
          break;
        }
      }
      if (find == false) {
        delete_abs_regions.push_back(abs_region);
      }
    }

    for (auto &abs_region : delete_abs_regions) {
      slice_frame.active.erase(abs_region);
    }

    return true;
  }

 private:
  std::vector<Dyninst::AbsRegion> _rhs;
};


static void
trackDependency
(
 const std::map<int, GPUParse::InstructionStat *> &inst_stat_map,
 Dyninst::Address inst_addr,
 std::map<int, int> &predicate_map,
 Dyninst::NodeIterator exit_node_iter,
 GPUParse::InstructionStat *inst_stat,
 int barriers,
 int step
)
{
  if (step >= TRACK_LIMIT) {
    return;
  }
  Dyninst::NodeIterator in_begin, in_end;
  (*exit_node_iter)->ins(in_begin, in_end);
  for (; in_begin != in_end; ++in_begin) {
    auto slice_node = boost::dynamic_pointer_cast<Dyninst::SliceNode>(*in_begin);
    auto addr = slice_node->addr();
    auto *slice_inst = inst_stat_map.at(addr);

    if (DEBUG_INSTRUCTION_ANALYZER) {
      std::cout << "find inst_addr " << inst_addr << " <- addr: " << addr;
    }

    Dyninst::Assignment::Ptr aptr = slice_node->assign();
    auto reg = aptr->out().absloc().reg();
    auto reg_id = reg.val() & 0xFF;

    for (size_t i = 0; i < inst_stat->srcs.size(); ++i) {
      if (reg_id == inst_stat->srcs[i]) {
        auto beg = inst_stat->assign_pcs[reg_id].begin();
        auto end = inst_stat->assign_pcs[reg_id].end();
        if (std::find(beg, end, addr) == end) {
          inst_stat->assign_pcs[reg_id].push_back(addr);
        }
        break;
      }
    }

    if (DEBUG_INSTRUCTION_ANALYZER) {
      std::cout << " reg " << reg_id << std::endl;
    }

    if (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_NONE && barriers == -1) {
      // 1. No predicate, stop immediately
    } else if (inst_stat->predicate == slice_inst->predicate &&
        inst_stat->predicate_flag == slice_inst->predicate_flag && barriers == -1) {
      // 2. Find an exact match, stop immediately
    } else {
      if (((slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_TRUE &&
              predicate_map[-(slice_inst->predicate + 1)] > 0) ||
            (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_FALSE &&
             predicate_map[(slice_inst->predicate + 1)] > 0)) && barriers == -1) {
        // 3. Stop if find both !@PI and @PI=
        // add one to avoid P0
      } else {
        // 4. Continue search
        if (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_TRUE) {
          predicate_map[slice_inst->predicate + 1]++;
        } else {
          predicate_map[-(slice_inst->predicate + 1)]++;
        }

        trackDependency(inst_stat_map, inst_addr, predicate_map, in_begin, inst_stat,
            barriers, step + 1);

        // Clear
        if (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_TRUE) {
          predicate_map[slice_inst->predicate + 1]--;
        } else {
          predicate_map[-(slice_inst->predicate + 1)]--;
        }
      }
    }
  }
}


static void
sliceIntelInstructions
(
 const Dyninst::ParseAPI::CodeObject::funclist &func_set,
 std::vector<GPUParse::Function *> &functions,
 int threads
)
{
  // Build a instruction map
  std::map<int, GPUParse::InstructionStat *> inst_stat_map;
  std::map<int, GPUParse::Block*> inst_block_map;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          auto *inst_stat = inst->inst_stat;
          inst_stat_map[inst->offset] = inst_stat;
          inst_block_map[inst->offset] = block;
        }
      }
    }
  }
  std::vector<std::pair<Dyninst::ParseAPI::GPUBlock *, Dyninst::ParseAPI::Function *>> block_vec;
  for (auto dyn_func : func_set) {
    for (auto *dyn_block : dyn_func->blocks()) {
      block_vec.emplace_back(static_cast<Dyninst::ParseAPI::GPUBlock*>(dyn_block), dyn_func);
    }
  }

  // Prepare pass: create instruction cache for slicing
  Dyninst::AssignmentConverter ac(true, false);
  Dyninst::Slicer::InsnCache dyn_inst_cache;

#pragma omp parallel for schedule(dynamic) firstprivate(ac, dyn_inst_cache) shared(block_vec, inst_stat_map) default(none) num_threads(threads)
  for (size_t i = 0; i < block_vec.size(); ++i) {
    Dyninst::GraphPtr g;
    Dyninst::NodeIterator exit_begin, exit_end;
    ParseAPI::GPUBlock *dyn_block = block_vec[i].first;
    auto *dyn_func = block_vec[i].second;

    Dyninst::ParseAPI::Block::Insns insns;
    dyn_block->enable_latency_blame();
    dyn_block->getInsns(insns);

    // used to keep track of assignments that are already backwardsliced (to avoid redundant computations)
    std::unordered_set<size_t> sliced;

    for (auto &inst_iter : insns) {
      auto &inst = inst_iter.second;
      auto inst_addr = inst_iter.first;
      auto *inst_stat = inst_stat_map.at(inst_addr);

      if (DEBUG_INSTRUCTION_ANALYZER) {
        //std::cout << "try to find inst_addr " << inst_addr << std::endl;
      }

      std::vector<Dyninst::Assignment::Ptr> assignments;
      ac.convert(inst, inst_addr, dyn_func, dyn_block, assignments);

      for (auto a : assignments) {
        std::hash<std::string> str_hasher;
        size_t input_hash = a->addr();
        for (auto i: a->inputs()) {
          input_hash += str_hasher(i.format());
        }
        if (input_hash != 0 && sliced.find(input_hash) != sliced.end()) {
          continue;
        } else {
          sliced.insert(input_hash);
        }
#ifdef FAST_SLICING
        FirstMatchPred p;
#else
        IgnoreRegPred p(a->inputs());
#endif
        Dyninst::Slicer s(a, dyn_block, dyn_func, &ac, &dyn_inst_cache);
        g = s.backwardSlice(p);
        g->exitNodes(exit_begin, exit_end);

        for (; exit_begin != exit_end; ++exit_begin) {
          std::map<int, int> predicate_map;
          // DFS to iterate the whole dependency graph
          if (inst_stat->predicate_flag == GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
            predicate_map[inst_stat->predicate + 1]++;
          } else if (inst_stat->predicate_flag == GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
            predicate_map[-(inst_stat->predicate + 1)]++;
          }
#ifdef FAST_SLICING
          TRACK_LIMIT = 1;
#endif
          auto barrier_threshold = inst_stat->barrier_threshold;
          trackDependency(inst_stat_map, inst_addr, predicate_map, exit_begin, inst_stat,
                          barrier_threshold, 0);
        }
      }
    }
  }
}


static void
createDefUseEdges
(
 std::vector<GPUParse::Function *> functions,
 std::string filePath
)
{
  std::map<uint64_t , std::map<uint64_t, uint32_t>> def_use_graph;

  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *_inst : block->insts) {
        auto *inst = _inst->inst_stat;
        int to = inst->pc;
        for (auto reg_vector: inst->assign_pcs) {
          for (int from: reg_vector.second) {
            uint32_t path_length = 0;
            if (def_use_graph.find(from) == def_use_graph.end()) {
              path_length = 1;
            } else {
              std::map<uint64_t, uint32_t> &from_incoming_edges = def_use_graph[from];
              for (auto edge: from_incoming_edges) {
                if (edge.second > path_length) {
                  path_length = edge.second;
                }
              }
            }
            def_use_graph[to][from] = path_length + 1;
          }
        }
      }
    }
  }
  std::ofstream file(filePath);
  if(file.is_open()) {
    for (auto iter: def_use_graph) {
      int to = iter.first;
      for (auto from: iter.second) {
        file << from.first << " -> " << to << std::endl;
      }
    }
    file.close();
  } else {
    std::cout << "could not open file\n";
  }
}


static uint32_t
addrToOffset
(
  Address function_start,
  Address addr
)
{
  return addr - function_start;
}


static uint32_t
addrToOffset
(
  Address function_start,
  int32_t addr
)
{
  uint32_t function_start_lower_bits = function_start;
  uint32_t addr_lower_bits = addr;
  uint32_t offset = addr_lower_bits - function_start_lower_bits;

  return offset;
}

  
static void
getJumpTargetOffsets
(
  KernelView &kv,
  int32_t offset,
  Address function_start,
  std::vector<uint32_t> &jump_target_offsets
)
{
  int32_t targets[KV_MAX_TARGETS_PER_INSTRUCTION];
  //----------------------------------------------------------------------
  // Intel claims to be returning absolute addresses for jump target PCs.
  // However, we have observed zeBinaries with function symbols
  // that have 48 bit addresses. Thus, absolute addresses for jump targets
  // in such functions won't fit into 32 bits!!! Nor is it appropriate to
  // use SIGNED numbers for jump target absolute addresses.
  //
  // Note: despite what Intel's documentation says, the targets are 32-bit
  // unsigned values
  //----------------------------------------------------------------------
  size_t count = kv.getInstTargets(offset, targets);

  // convert 32-bit absolute unsigned addresses into relative offsets from
  // function_start for convenience
  for (unsigned int i = 0; i < count; i++) {
    jump_target_offsets.push_back(targets[i]);
  }
}


static void
ensureBlockIsTarget
(
  std::vector<GPUParse::Target*> &targets,
  GPUParse::Block *block,
  GPUParse::Inst *inst,
  TargetType type 
)
{
  bool blockIsTarget = false;
  for (auto *target : targets) {
    if (target->block == block) {
      blockIsTarget = true;
      break;
    }
  }

  if (!blockIsTarget) {
    targets.push_back(new GPUParse::Target(inst, block, type));
  }
}


void
IntelGPUInstDumper::dump(GPUParse::Inst* inst)
{
  auto inst_offset = addrToOffset(function.address, inst->offset);
  size_t n = kv.getInstSyntax(inst_offset, NULL, 0);
  assert(n < MAX_STR_SIZE);

  char inst_str[MAX_STR_SIZE];
  inst_str[n] = '\0';
  auto fmt_opts = IGA_FORMATTING_OPTS_DEFAULT; // see iga.h
  kv.getInstSyntax(inst_offset, inst_str, n, fmt_opts);

  std::cout << "      " << std::hex << inst->offset << std::dec << ": " << inst_str << std::endl;
}


static void
recoverIntelCFG
(
 char *fn_start,
 size_t fn_length,
 GPUParse::Function &function,
 bool du_graph_wanted
)
{
  KernelView kv(IGA_XE_HPC, fn_start, fn_length);
  std::map<int, GPUParse::Block *> block_offset_map;

  Address offset = 0;
  Address endOffset = fn_length;

  unsigned long block_id = 0;

  // construct basic blocks
  while (offset < endOffset) {
    Address position = offset + function.address;
    auto *block =
      new GPUParse::Block(block_id, position,
        function.name + "_" + std::to_string(block_id));
    block_id++;

    function.blocks.push_back(block);
    block_offset_map[offset] = block;

    auto size = kv.getInstSize(offset);
    GPUParse::Inst *inst;
    if (du_graph_wanted) {
      auto *inst_stat = getIntelInstructionStat(kv, offset);
      inst = new GPUParse::IntelInst(position, size, inst_stat);
    } else {
      inst = new GPUParse::IntelInst(position, size);
    }
    block->insts.push_back(std::move(inst));

    while (!kv.isInstTarget(offset + size) && (offset + size < fn_length)) {
      offset += size;
      size = kv.getInstSize(offset);
      if (size == 0) {
        // this is a weird edge case, what to do?
        break;
      }

      auto *inst_stat = getIntelInstructionStat(kv, offset);
      position = offset + function.address;
      inst = new GPUParse::IntelInst(position, size, inst_stat);
      block->insts.push_back(std::move(inst));
    }

    auto opcode = kv.getOpcode(offset);
    if (opcode == iga::Op::CALL ||  opcode == iga::Op::CALLA) {
      inst->is_call = true;
    }

    offset += size;
  }

  // construct targets
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    auto *block = function.blocks[i];
    auto *inst = block->insts.back();

    auto inst_offset = addrToOffset(function.address, inst->offset);

    // obtain offsets of jump targets, if any
    std::vector<uint32_t> jump_target_offsets;
    getJumpTargetOffsets(kv, inst_offset, function.address, jump_target_offsets);

    auto opCodeGroup = kv.getOpcodeGroup(inst_offset);
    bool eot_inst = opCodeGroup == KV_OPGROUP_SEND_EOT;
    bool pred_inst = kv.getPredicate(inst_offset) != iga::PredCtrl::NONE;

    // consider adding a fall through edge unless this is an end of thread (EOT)
    // instruction that is not predicated
    if (!eot_inst || pred_inst) {
      if (i != function.blocks.size() - 1) {
        // Add a fall through edge
        // The last block and the end of thread (EOT) block do not have a fall through
        int next_block_offset =
          addrToOffset(function.address, function.blocks[i + 1]->insts.front()->offset);

        bool while_inst = opCodeGroup == KV_OPGROUP_WHILE;
        bool join_inst = kv.getOpcode(inst_offset) == iga::Op::JOIN;
        if ((pred_inst || while_inst || jump_target_offsets.size() == 0) || join_inst) {
          jump_target_offsets.push_back(next_block_offset);
        }
      }
    }

    for (size_t j = 0; j < jump_target_offsets.size(); j++) {
      auto *target_block = block_offset_map.at(jump_target_offsets[j]);

      TargetType type = TargetType::COND_TAKEN;
      if (inst->is_call) {
        // call fall through edge to next instruction
        type = TargetType::CALL_FT;
      } else if (target_block->insts.front()->offset == inst->offset + inst->size) {
        // fall through edge to target_block, which immediately follows instruction inst
        type = TargetType::DIRECT;
      }

      ensureBlockIsTarget(block->targets, target_block, inst, type);
    }
  }

  if (DEBUG_CFG) {
    IntelGPUInstDumper intelGPUInstDumper(function, kv);
    dumpFunction(function, intelGPUInstDumper);
  }
}


static void
parseGPUFunction
(
 std::vector<GPUParse::Function *> &functions,
 char *fn_start,
 size_t fn_length,
 std::string &fn_name,
 Offset fn_address,
 bool du_graph_wanted
)
{
  if (fn_length > 0) {
    GPUParse::Function *function = new GPUParse::Function(0, fn_name.c_str(), fn_address);
    recoverIntelCFG(fn_start, fn_length, *function, du_graph_wanted);
    functions.push_back(function);
  }
}


static void
exportCfgIntoDyninst
(
  std::vector<GPUParse::Function *> &functions,
  Dyninst::SymtabAPI::Symtab *the_symtab,
  Dyninst::ParseAPI::CodeSource **code_src,
  Dyninst::ParseAPI::CodeObject **code_obj
)
{
  CFGFactory *cfg_fact = new GPUCFGFactory(functions);
  *code_src = new GPUCodeSource(functions, the_symtab);
  *code_obj = new CodeObject(*code_src, cfg_fact);
}


static void
recoverCfgPatchTokenBinary
(
  const std::string &search_path,
  ElfFile *elfFile,
  bool du_graph_wanted,
  std::string &fn_name,
  Dyninst::SymtabAPI::Symtab *the_symtab,
  Dyninst::ParseAPI::CodeSource **code_src,
  Dyninst::ParseAPI::CodeObject **code_obj,
  std::vector<GPUParse::Function *> &functions
)
{
  Address fn_address = 0;
  char *fn_text = NULL;
  auto fn_size = elfFile->getTextSection(&fn_text);
  parseGPUFunction(functions, fn_text, fn_size, fn_name, fn_address, du_graph_wanted);
}


static void
recoverCfgZeBinary
(
  const std::string &search_path,
  ElfFile *elfFile,
  bool du_graph_wanted,
  Dyninst::SymtabAPI::Symtab *the_symtab,
  Dyninst::ParseAPI::CodeSource **code_src,
  Dyninst::ParseAPI::CodeObject **code_obj,
  std::vector<GPUParse::Function *> &functions
)
{
  std::vector<Symbol *> symbols;
  the_symtab->getAllSymbolsByType(symbols, Symbol::ST_FUNCTION);
  for (auto &symbol : symbols) {
    Region *region = symbol->getRegion();
    char *fn_text = ((char *) region->getPtrToRawData()) +
      (symbol->getOffset() - region->getMemOffset());;
    auto fn_size = symbol->getSize();
    auto fn_name = symbol->getTypedName();
    auto fn_address = symbol->getOffset();
    if (fn_name != "_entry") {
      parseGPUFunction(functions, fn_text, fn_size, fn_name, fn_address, du_graph_wanted);
    }
  }
}


//******************************************************************************
// interface functions
//******************************************************************************

bool
buildIntelGPUCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 bool cfg_wanted,
 bool du_graph_wanted,
 int jobs,
 Dyninst::ParseAPI::CodeSource **code_src,
 Dyninst::ParseAPI::CodeObject **code_obj
)
{
  Region *reg = NULL;
  std::vector<GPUParse::Function *> functions;

  bool isZeBinary = the_symtab->findRegion(reg, ".ze_info");
  if (isZeBinary) {
    if (cfg_wanted) {
      recoverCfgZeBinary(search_path, elfFile, du_graph_wanted, the_symtab,
                         code_src, code_obj, functions);
    }
  } else {
    // An Intel GPU binary for a kernel does not contain a function symbol for the kernel
    // in its symbol table. Without a function symbol in the symbol table, Dyninst will not
    // associate line map entries with addresses in the kernel. To cope with this defect of
    // binaries for Intel GPU kernels, we add a function symbol for the kernel to its Dyninst
    // symbol table.
    auto fn_name = elfFile->getGPUKernelName();
    addCustomFunctionObject(fn_name, the_symtab); //adds a dummy function object

    if (cfg_wanted) {
      recoverCfgPatchTokenBinary(search_path, elfFile, du_graph_wanted, fn_name,
                                 the_symtab, code_src, code_obj, functions);
    }
  }

  bool builtCFG = functions.size() > 0;

  if (builtCFG) {
    exportCfgIntoDyninst(functions, the_symtab, code_src, code_obj);
    if (du_graph_wanted) {
      std::string elf_filePath = elfFile->getFileName();
      const char *delimiter = ".gpubin";
      size_t delim_loc = elf_filePath.find(delimiter);
      std::string du_filePath = elf_filePath.substr(0, delim_loc + 7);
      du_filePath += ".du";
      sliceIntelInstructions((*code_obj)->funcs(), functions, jobs);
      createDefUseEdges(functions, du_filePath);
    }
  } else {
    *code_src = new SymtabCodeSource(the_symtab);
    *code_obj = new CodeObject(*code_src, NULL, NULL, false, true);
  }

  // parse function ranges eagerly here because doing it lazily
  // causes a race on Dyninst::Symtab::func_lookup
  the_symtab->parseFunctionRanges();

  return builtCFG;
}


#endif // ENABLE_IGC
