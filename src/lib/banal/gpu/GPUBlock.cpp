#include "GPUBlock.hpp"
#include <Instruction.h>
#include <Operand.h>
#include <Operation_impl.h>
#include <Register.h>

#include <include/hpctoolkit-config.h>

#define MAX_INST_SIZE 32




namespace Dyninst {
namespace ParseAPI {

static void
appendOperandstoInst
(
 GPUParse::Inst *inst,
 InstructionAPI::Instruction instruction
)
{
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

  if (inst->inst_stat->dsts.size() == 0) {
    std::cout << "dst.size == 0\n";
    // Fake register
    MachRegister r(128 | intel::GPR | Arch_intelGen9);
    InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
    instruction.appendOperand(reg_ptr, false, true);
  } else {
    std::cout << "dst.size != 0\n";
    for (auto dst : inst->inst_stat->dsts) {
      if (dst != -1) {
        MachRegister r(dst | intel::GPR | Arch_intelGen9);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
        instruction.appendOperand(reg_ptr, false, true);
      }
    }
  }

  for (auto src : inst->inst_stat->srcs) {
    if (src != -1) {
      MachRegister r(src | intel::GPR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, true, false);
    }
  }

  for (auto bsrc : inst->inst_stat->bsrcs) {
    if (bsrc != -1) {
      MachRegister r(bsrc | intel::BR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, true, false);
    }
  }
}


GPUBlock::GPUBlock(CodeObject * o, CodeRegion * r,
  Address start, Address end, Address last, 
  std::vector<GPUParse::Inst *> insts,
  Dyninst::Architecture arch) :
  Block(o, r, start, end, last),  _arch(arch) {
    for (auto *inst: insts) {
      _insts.push_back(inst);
    }
  }

#if 0
Address GPUBlock::last() const {
  return this->_insts.back()->offset;
}
#endif


void GPUBlock::getInsns(Insns &insns) const {
  unsigned char dummy_inst[MAX_INST_SIZE];
//#ifdef DYNINST_SUPPORTS_INTEL_GPU
  entryID entry_id = _entry_ids_max_;

  for (auto &ins : _insts) {
    entryID entry_id = intel_gpu_op_general;

    const std::string &op_class = ins->inst_stat->op;
    if (op_class.find("CALL") != std::string::npos) {
      entry_id = intel_gpu_op_call;
    }

    auto offset = ins->offset;
    auto size = ins->size;

#ifdef DYNINST_SUPPORTS_INTEL_GPU
    InstructionAPI::Operation op(entry_id, "", _arch);
#else
    // Dyninst 10.2.0 does not have cuda_op_general
    InstructionAPI::Operation op;
#endif

    InstructionAPI::Instruction inst(op, size, dummy_inst, _arch);
    appendOperandstoInst(ins, inst);
    insns.emplace(offset, inst);
  }
}

}
}
