#ifndef BANAL_GPU_GPU_BLOCK_H
#define BANAL_GPU_GPU_BLOCK_H

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT GPUBlock : public Block {
 public:
  GPUBlock(CodeObject * o, CodeRegion * r, Address start, std::vector<Offset> &offsets);

  virtual ~GPUBlock() {}

  virtual void getInsns(Insns &insns) const;

  virtual Address last() const;

 private:
  std::vector<Offset> _inst_offsets;
};

}
}

#endif
