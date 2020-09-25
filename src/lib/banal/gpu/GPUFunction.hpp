#ifndef BANAL_GPU_GPU_FUNCTION_H
#define BANAL_GPU_GPU_FUNCTION_H

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT GPUFunction : public ParseAPI::Function {
 public:
  GPUFunction(Address addr, std::string name, CodeObject * obj, 
    CodeRegion * region, InstructionSource * isource) :
    Function(addr, name, obj, region, isource) {
    _cache_valid = true;
  }

  virtual ~GPUFunction() {}

  void setEntry(Block *entry);
};

}
}

#endif
