#ifndef BANAL_INTEL_INTEL_FUNCTION_HPP
#define BANAL_INTEL_INTEL_FUNCTION_HPP

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT IntelFunction : public ParseAPI::Function {
 public:
  IntelFunction(Address addr, std::string name, CodeObject * obj, 
    CodeRegion * region, InstructionSource * isource) :
    Function(addr, name, obj, region, isource) {
    _cache_valid = true;
  }

  virtual ~IntelFunction() {}

  void setEntry(Block *entry);
};

}
}

#endif
