#ifndef BANAL_INTEL_INTEL_CFG_FACTORY_HPP
#define BANAL_INTEL_INTEL_CFG_FACTORY_HPP

#include <CFGFactory.h>
#include <unordered_map>

#include "IntelBlock.hpp"
#include "../cuda/DotCFG.hpp"

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT IntelCFGFactory : public CFGFactory {   
 public:
  IntelCFGFactory(std::vector<CudaParse::Function *> &functions) :
    _functions(functions) {}
  virtual ~IntelCFGFactory() {}

 protected:
  virtual Function * mkfunc(Address addr, FuncSource src, 
    std::string name, CodeObject * obj, CodeRegion * region,
    Dyninst::InstructionSource * isrc);

 private:
  std::vector<CudaParse::Function *> &_functions;
  std::unordered_map<size_t, IntelBlock *> _block_filter; 
};

}
}

#endif
