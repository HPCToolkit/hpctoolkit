#ifndef BANAL_INTEL_INTEL_BLOCK_HPP
#define BANAL_INTEL_INTEL_BLOCK_HPP

#include <CFG.h>

namespace Dyninst {
namespace ParseAPI {

class PARSER_EXPORT IntelBlock : public Block {
 public:
  IntelBlock(CodeObject * o, CodeRegion * r, Address start, std::vector<Offset> &offsets);

  virtual ~IntelBlock() {}

  virtual void getInsns(Insns &insns) const;

  virtual Address last() const;

 private:
  std::vector<Offset> _inst_offsets;
};

}
}

#endif
