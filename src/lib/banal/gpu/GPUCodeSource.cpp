#include "GPUCodeSource.hpp"

namespace Dyninst {
namespace ParseAPI {

GPUCodeSource::GPUCodeSource(
  std::vector<GPUParse::Function *> &functions, Dyninst::SymtabAPI::Symtab *s) {
  for (auto *function : functions) {
    Address address = function->address;
    _hints.push_back(Hint(address, 0, 0, function->name));
  }
}

}
}
