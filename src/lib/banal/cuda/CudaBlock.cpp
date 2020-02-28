#include "CudaBlock.hpp"
#include <Instruction.h>


namespace Dyninst {
namespace ParseAPI {

CudaBlock::CudaBlock(CodeObject * o, CodeRegion * r,
  Address start, std::vector<Offset> &offsets) : Block(o, r, start) {
  for (auto offset : offsets) {
    _inst_offsets.push_back(offset);
  }
}


Address CudaBlock::last() const {
  return this->_inst_offsets.back();
}


void CudaBlock::getInsns(Insns &insns) const {
  for (auto offset : _inst_offsets) {
#ifdef DYNINST_INSTRUCTION_PTR
    insns.insert(std::pair<long unsigned int, 
      InstructionAPI::InstructionPtr>(offset, NULL));
#else
    InstructionAPI::Instruction inst;    
    insns[offset] = inst;
#endif
  }
}

}
}
