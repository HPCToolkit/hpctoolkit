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
  entryID entry_id = _entry_ids_max_;
  unsigned char dummy_inst[MAX_INST_SIZE];

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

  for (auto &inst_offset : _inst_offsets) {
#ifdef DYNINST_SUPPORTS_INTEL_GPU
    entryID entry_id = _entry_ids_max_;
    InstructionAPI::Operation op(entry_id, "", _arch);

    auto offset = inst_offset.first;
    auto size = inst_offset.second;

    InstructionAPI::Instruction inst(op, size, dummy_inst, _arch);
#else
    // Dyninst 10.2.0 does not have cuda_op_general
    // Just use the default constructor
    InstructionAPI::Instruction inst;    
#endif  // DYNINST_SUPPORTS_INTEL_GPU  
    insns.emplace(offset, inst);
  }
}

}
}
