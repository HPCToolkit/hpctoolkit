#include "CudaCodeSource.hpp"

namespace Dyninst {
namespace ParseAPI {

CudaCodeSource::CudaCodeSource(std::vector<CudaParse::Function *> &functions, Dyninst::SymtabAPI::Symtab *s)  /* : SymtabCodeSource(s) */  {
  for (auto *function : functions) {
    int address = function->address;
    _hints.push_back(Hint(address, 0, 0, function->name));
  }
}

}
}
