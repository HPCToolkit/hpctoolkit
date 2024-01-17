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


//***************************************************************************
// Dyninst includes
//***************************************************************************


#include <Instruction.h>
#include <Operand.h>
#include <Operation_impl.h>
#include <Register.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "hpctoolkit-config.h"
#include "GPUBlock.hpp"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0
#define MAX_INST_SIZE 32



//******************************************************************************
// begin namespaces
//******************************************************************************

namespace Dyninst {
namespace ParseAPI {


//******************************************************************************
// local operations
//******************************************************************************

static void
appendOperandstoInst
(
 GPUParse::Inst *inst,
 InstructionAPI::Instruction& instruction
)
{
#if 0 // intel::PR and intel::GPR are not defined -- johnmc
  // Instruction predicate flags
    int pred = 0;// single pred register;
    if (inst->inst_stat->predicate_flag ==
        GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
      MachRegister r(pred | intel::PR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      // bool isRead, bool isWritten, bool isImplicit, bool trueP, bool falseP
      instruction.appendOperand(reg_ptr, true, false, false, true, false);
    } else if (inst->inst_stat->predicate_flag ==
               GPUParse::InstructionStat::PredicateFlag::PREDICATE_FALSE) {
      MachRegister r(pred | intel::PR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      // bool isRead, bool isWritten, bool isImplicit, bool trueP, bool falseP
      instruction.appendOperand(reg_ptr, true, false, false, false, true);
    }

#if DEBUG
    std::cout << "dst register: ";
#endif
    if (inst->inst_stat->dsts.size() == 0)
    {
#if DEBUG
      std::cout << 128;
#endif
      // Fake register
      MachRegister r(128 | intel::GPR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, false, true);
  } else {
    for (auto dst : inst->inst_stat->dsts) {
      if (dst != -1) {
#if DEBUG
        std::cout << dst << ", ";
#endif
        MachRegister r(dst | intel::GPR | Arch_intelGen9);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
        instruction.appendOperand(reg_ptr, false, true);
      }
    }
  }
#if DEBUG
  std::cout << std::endl;
#endif

#if DEBUG
  std::cout << "src register: ";
#endif
  for (auto src : inst->inst_stat->srcs) {
    if (src != -1) {
#if DEBUG
      std::cout << src << ", ";
#endif
      MachRegister r(src | intel::GPR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, true, false);
    }
  }

#if DEBUG
  std::cout << std::endl;
#endif
#else
      abort();
#endif
}

//******************************************************************************
// interface operations
//******************************************************************************


GPUBlock::GPUBlock(CodeObject * o, CodeRegion * r,
  Address start, Address end, Address last,
  std::vector<GPUParse::Inst*> insts,
  Dyninst::Architecture arch) :
  Block(o, r, start, end, last), _insts(std::move(insts)), _arch(arch) {}


void GPUBlock::getInsns(Insns &insns) const {
  unsigned char dummy_inst[MAX_INST_SIZE];
#ifdef DYNINST_SUPPORTS_INTEL_GPU
  entryID entry_id = _entry_ids_max_;

  if (_arch == Arch_cuda) {
    entry_id = cuda_op_general;
  } else if (_arch == Arch_intelGen9) {
    entry_id = intel_gpu_op_general;
  }

  // Don't construct CFG if Dyninst does not support this GPU arch
  if (entry_id == _entry_ids_max_) {
    return;
  }
#endif  // DYNINST_SUPPORTS_INTEL_GPU

  for (auto &ins : _insts) {

    auto offset = ins->offset;
    auto size = ins->size;

#ifdef DYNINST_SUPPORTS_INTEL_GPU
    InstructionAPI::Operation op(entry_id, "", _arch);
#else
    // Dyninst 10.2.0 does not have cuda_op_general
    InstructionAPI::Operation op;
#endif

    InstructionAPI::Instruction inst(op, size, dummy_inst, _arch);
    if (latency_blame_enabled) {
      appendOperandstoInst(ins, inst);
    }
    insns.emplace(offset, inst);
  }
}


void GPUBlock::enable_latency_blame() {
  latency_blame_enabled = true;
}



//******************************************************************************
// end namespaces
//******************************************************************************

}
}
