#ifndef _CUDA_FUNCTION_H_
#define _CUDA_FUNCTION_H_

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT CudaFunction : public ParseAPI::Function {
 public:
  CudaFunction(Address addr, std::string name, CodeObject * obj, 
    CodeRegion * region, InstructionSource * isource) :
    Function(addr, name, obj, region, isource) {
    _cache_valid = true;
  }

  virtual ~CudaFunction() {}

  void setEntry(Block *entry);
};

}
}

#endif
