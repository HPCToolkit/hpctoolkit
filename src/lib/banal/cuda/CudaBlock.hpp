#ifndef _CUDA_BLOCK_H_
#define _CUDA_BLOCK_H_

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT CudaBlock : public Block {
 public:
  CudaBlock(CodeObject * o, CodeRegion * r, Address start, std::vector<Offset> &offsets);

  virtual ~CudaBlock() {}

  virtual void getInsns(Insns &insns) const;

  virtual Address last() const;

 private:
  std::vector<Offset> _inst_offsets;
};

}
}

#endif
