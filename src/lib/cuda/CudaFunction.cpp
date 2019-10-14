#include "CudaFunction.hpp"

namespace Dyninst {
namespace ParseAPI {

void CudaFunction::setEntry(Block *entry) {
  _region = entry->region();
  _entry = entry;
}

}
}
