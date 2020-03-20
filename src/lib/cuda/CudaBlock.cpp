#include "CudaBlock.hpp"
#include <Instruction.h>
#include <Register.h>
#include <Operand.h>
#include <Operation_impl.h>

namespace Dyninst {
namespace ParseAPI {

CudaBlock::CudaBlock(CodeObject * o, CodeRegion * r,
  CudaParse::Block * block) : Block(o, r, block->address) {
  for (auto *inst : block->insts) {
    _insts.push_back(inst);
  }
}


Address CudaBlock::last() const {
  return this->_insts.back()->offset;
}


void CudaBlock::getInsns(Insns &insns) const {
  // Manually decoding cuda instructions
  for (auto *inst : _insts) {
    entryID entry_id = cuda_op_general;

    const std::string &op_class = inst->inst_stat->op;
    if (op_class.find("CALL") != std::string::npos) {
      entry_id = cuda_op_call;
    }

    InstructionAPI::Operation op(entry_id, op_class, Arch_cuda);

    // TODO(Keren): sm_60-> size 8
    // It does not matter now because hpcstruct explicitly set length of instructions
    InstructionAPI::Instruction instruction(op, 16, NULL, Arch_cuda);

    // Instruction predicate flags
    int pred = inst->inst_stat->predicate;
    if (inst->inst_stat->predicate_flag == CudaParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
      MachRegister r(pred | cuda::PR | Arch_cuda);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      // bool isRead, bool isWritten, bool isImplicit, bool trueP, bool falseP
      instruction.appendOperand(reg_ptr, true, false, false, true, false);
    } else if (inst->inst_stat->predicate_flag == CudaParse::InstructionStat::PredicateFlag::PREDICATE_FALSE) {
      MachRegister r(pred | cuda::PR | Arch_cuda);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      // bool isRead, bool isWritten, bool isImplicit, bool trueP, bool falseP
      instruction.appendOperand(reg_ptr, true, false, false, false, true);
    }

    if (inst->inst_stat->dsts.size() == 0) {
      // Fake register
      MachRegister r(256 | cuda::GPR | Arch_cuda);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, false, true);
    } else {
      for (auto dst : inst->inst_stat->dsts) {
        if (dst != -1) {
          MachRegister r(dst | cuda::GPR | Arch_cuda);
          InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
          instruction.appendOperand(reg_ptr, false, true);
        }   
      } 
    }   

    for (auto pdst : inst->inst_stat->pdsts) {
      if (pdst != -1) {
        MachRegister r(pdst | cuda::PR | Arch_cuda);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
        instruction.appendOperand(reg_ptr, false, true);
      }
    } 

    for (auto src : inst->inst_stat->srcs) {
      if (src != -1) {
        MachRegister r(src | cuda::GPR | Arch_cuda);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
        instruction.appendOperand(reg_ptr, true, false);
      }
    }

    for (auto psrc : inst->inst_stat->psrcs) {
      if (psrc != -1) {
        MachRegister r(psrc | cuda::PR | Arch_cuda);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
        instruction.appendOperand(reg_ptr, true, false);
      }
    }

    insns[inst->offset] = instruction;
  }
}

}
}
