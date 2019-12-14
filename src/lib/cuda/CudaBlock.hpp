#ifndef _CUDA_CUDA_BLOCK_H_
#define _CUDA_CUDA_BLOCK_H_

#include <CFG.h>

#include "DotCFG.hpp"

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT CudaBlock : public Block {
 public:
  CudaBlock(CodeObject * o, CodeRegion * r, CudaParse::Block * block);

  virtual ~CudaBlock() {}

  virtual void getInsns(Insns &insns) const;

  virtual Address last() const;

 private:
  std::vector<CudaParse::Instruction *> _insts;
};

}
}

#endif
