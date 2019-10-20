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
    InstructionAPI::Operation *operation = NULL;
    const std::string &op_class = inst->inst_stat->op;

    if (op_class.find("CALL") != std::string::npos) {
      operation = new InstructionAPI::Operation(cuda_op_call, op_class, Arch_cuda);
    } else {
      operation = new InstructionAPI::Operation(cuda_op_general, op_class, Arch_cuda);
    } 

    // TODO(Keren): sm_60-> size 8
#ifdef DYNINST_INSTRUCTION_PTR
    InstructionAPI::InstructionPtr instruction_ptr(
      new InstructionAPI::Instruction(*operation, 16, NULL, Arch_cuda));
#else
    InstructionAPI::Instruction instruction(*operation, 16, NULL, Arch_cuda);
#endif

    int dst = inst->inst_stat->dst;
    if (dst != -1) {
      MachRegister r(dst | cuda::GPR | Arch_cuda);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
#ifdef DYNINST_INSTRUCTION_PTR
      instruction_ptr->appendOperand(reg_ptr, true, false);
#else
      instruction.appendOperand(reg_ptr, true, false);
#endif
    }

    for (auto src : inst->inst_stat->srcs) {
      if (src != -1) {
        MachRegister r(src | cuda::GPR | Arch_cuda);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
#ifdef DYNINST_INSTRUCTION_PTR
        instruction_ptr.appendOperand(reg_ptr, false, true);
#else
        instruction.appendOperand(reg_ptr, false, true);
#endif
      }
    }

#ifdef DYNINST_INSTRUCTION_PTR
    insns.insert(std::pair<long unsigned int, 
      InstructionAPI::InstructionPtr>(inst->offset, instruction_ptr));
#else
    insns[inst->offset] = instruction;
#endif
  }
}

}
}
