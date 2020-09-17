#include "IntelFunction.hpp"

namespace Dyninst {
namespace ParseAPI {

void IntelFunction::setEntry(Block *entry) {
  _region = entry->region();
  _entry = entry;
}

}
}
