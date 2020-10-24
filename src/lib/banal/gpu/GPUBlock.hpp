#ifndef BANAL_GPU_GPU_BLOCK_H
#define BANAL_GPU_GPU_BLOCK_H

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT GPUBlock : public Block {
 public:
  GPUBlock(CodeObject * o, CodeRegion * r, Address start,
    std::vector<std::pair<Offset, size_t>> &offsets, Architecture arch);

  virtual ~GPUBlock() {}

  virtual void getInsns(Insns &insns) const;

  virtual Address last() const;

 private:
  // <offset, size> pair
  std::vector<std::pair<Offset, size_t>> _inst_offsets;
  Architecture _arch;
};

}
}

#endif
