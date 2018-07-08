#include "CudaBlock.hpp"


namespace Dyninst {
namespace ParseAPI {

CudaBlock::CudaBlock(CodeObject * o, CodeRegion * r,
  Address start, std::vector<Offset> &offsets) : Block(o, r, start) {
  for (auto offset : offsets) {
    _inst_offsets.push_back(offset);
  }
}

void CudaBlock::getInsns(Insns &insns) const {
  for (auto offset : _inst_offsets) {
    insns.insert(std::pair<long unsigned int, 
      boost::shared_ptr<Dyninst::InstructionAPI::Instruction>>(offset, NULL));
  }
}

}
}
