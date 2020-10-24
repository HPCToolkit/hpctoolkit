#include "GPUBlock.hpp"
#include <Instruction.h>

#define MAX_INST_SIZE 32

namespace Dyninst {
namespace ParseAPI {

GPUBlock::GPUBlock(CodeObject * o, CodeRegion * r,
  Address start, std::vector<std::pair<Offset, size_t>> &offsets) :
  Block(o, r, start), _inst_offsets(offsets) {}


Address GPUBlock::last() const {
  return this->_inst_offsets.back().first;
}


void GPUBlock::getInsns(Insns &insns) const {
  unsigned char dummy_inst[MAX_INST_SIZE];

  for (auto &inst_offset : _inst_offsets) {
    entryID entry_id = intel_gpu_op_general;
    InstructionAPI::Operation op(entry_id, "", Arch_intelGen9);

    auto offset = inst_offset.first;
    auto size = inst_offset.second;

    InstructionAPI::Instruction inst(op, size, dummy_inst, Arch_intelGen9);
    insns.emplace(offset, inst);
  }
}

}
}
