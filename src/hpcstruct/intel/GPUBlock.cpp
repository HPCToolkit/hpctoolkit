// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

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
      abort();
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
