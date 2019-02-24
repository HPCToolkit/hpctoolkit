#include "CudaCodeSource.hpp"

namespace Dyninst {
namespace ParseAPI {

CudaCodeSource::CudaCodeSource(
  std::vector<CudaParse::Function *> &functions, Dyninst::SymtabAPI::Symtab *s) {
  for (auto *function : functions) {
    Address address = function->address;
    _hints.push_back(Hint(address, 0, 0, function->name));
  }
  
  // Add a NULL region to prevent dyninst crashed
  _regions.push_back(NULL);
}

}
}
