#include "GPUFunction.hpp"

namespace Dyninst {
namespace ParseAPI {

void GPUFunction::setEntry(Block *entry) {
  _region = entry->region();
  _entry = entry;
}

}
}
