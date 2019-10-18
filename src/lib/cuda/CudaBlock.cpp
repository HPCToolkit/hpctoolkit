#include "CudaBlock.hpp"
#include <Instruction.h>


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
  for (auto *inst : _insts) {
#ifdef DYNINST_INSTRUCTION_PTR
    insns.insert(std::pair<long unsigned int, 
      InstructionAPI::InstructionPtr>(inst->offset, NULL));
#else
    InstructionAPI::Instruction instruction;
    insns[inst->offset] = instruction;
#endif
  }
}

}
}
