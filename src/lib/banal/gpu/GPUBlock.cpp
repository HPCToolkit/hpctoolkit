#include "GPUBlock.hpp"
#include <Instruction.h>

#include <include/hpctoolkit-config.h>

#define MAX_INST_SIZE 32

namespace Dyninst {
namespace ParseAPI {

GPUBlock::GPUBlock(CodeObject * o, CodeRegion * r,
  Address start, std::vector<std::pair<Offset, size_t>> &offsets,
  Dyninst::Architecture arch) :
  Block(o, r, start), _inst_offsets(offsets), _arch(arch) {}


Address GPUBlock::last() const {
  return this->_inst_offsets.back().first;
}


void GPUBlock::getInsns(Insns &insns) const {
#ifdef DYNINST_SUPPORTS_INTEL_GPU
  unsigned char dummy_inst[MAX_INST_SIZE];

  for (auto &inst_offset : _inst_offsets) {
    entryID entry_id = intel_gpu_op_general;
    InstructionAPI::Operation op(entry_id, "", _arch);

    auto offset = inst_offset.first;
    auto size = inst_offset.second;

    InstructionAPI::Instruction inst(op, size, dummy_inst, _arch);
    insns.emplace(offset, inst);
  }
#endif  // DYNINST_SUPPORTS_INTEL_GPU
}

}
}
