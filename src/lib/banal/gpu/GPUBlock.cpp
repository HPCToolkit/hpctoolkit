#include "GPUBlock.hpp"
#include <Instruction.h>


namespace Dyninst {
namespace ParseAPI {

GPUBlock::GPUBlock(CodeObject * o, CodeRegion * r,
  Address start, std::vector<std::pair<Offset, size_t>> &offsets) :
  Block(o, r, start), _inst_offsets(offsets) {}


Address GPUBlock::last() const {
  return this->_inst_offsets.back().first;
}


void GPUBlock::getInsns(Insns &insns) const {
  for (auto &inst_offset : _inst_offsets) {
    entryID entry_id = intel_gpu_op_general;
    InstructionAPI::Operation op(entry_id, "", Arch_intelGen9);

    auto offset = inst_offset.first;
    auto size = inst_offset.second;

#if 0 
// No longer support this path
#ifdef DYNINST_INSTRUCTION_PTR
    insns.insert(std::pair<long unsigned int, 
      InstructionAPI::InstructionPtr>(offset, NULL));
#endif
#endif

    InstructionAPI::Instruction inst(op, size, NULL, Arch_intelGen9);
    insns.emplace(offset, std::move(inst));
  }
}

}
}
