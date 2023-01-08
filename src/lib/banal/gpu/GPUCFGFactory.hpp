#ifndef BANAL_GPU_GPU_CFG_FACTORY_H
#define BANAL_GPU_GPU_CFG_FACTORY_H

#include <CFGFactory.h>

#include "GPUBlock.hpp"
#include "DotCFG.hpp"

#include "tbb/concurrent_hash_map.h"

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT GPUCFGFactory : public CFGFactory {
 public:
  GPUCFGFactory(std::vector<GPUParse::Function *> &functions) :
    _functions(functions) {}
  virtual ~GPUCFGFactory() {}

 protected:
  virtual Function * mkfunc(Address addr, FuncSource src,
    std::string name, CodeObject * obj, CodeRegion * region,
    Dyninst::InstructionSource * isrc);

 private:
  std::vector<GPUParse::Function *> &_functions;
  tbb::concurrent_hash_map<size_t, GPUBlock *> _block_filter;
};

}
}

#endif
