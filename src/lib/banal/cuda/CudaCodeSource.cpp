#include "CudaCodeSource.hpp"

namespace Dyninst {
namespace ParseAPI {

CudaCodeSource::CudaCodeSource(std::vector<CudaParse::Function *> &functions) {
  for (auto *function : functions) {
    int offset = function->blocks[0]->insts[0]->offset;
    _hints.push_back(Hint(offset, 0, 0, function->name));
  }
}

}
}
