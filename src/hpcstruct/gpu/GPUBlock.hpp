// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef BANAL_GPU_GPU_BLOCK_H
#define BANAL_GPU_GPU_BLOCK_H

//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <CFG.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "GPUCFG.hpp"   // GPUParse



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {



//***************************************************************************
// type declarations
//***************************************************************************


class GPUBlock : public Block {
public:
  GPUBlock(CodeObject * o, CodeRegion * r,
    Address start, Address end, Address last,
    std::vector<GPUParse::Inst *> insts, Architecture arch);

  virtual ~GPUBlock() {}

  virtual void getInsns(Insns &insns) const;

  virtual void enable_latency_blame();

private:
  std::vector<GPUParse::Inst *> _insts;
  Architecture _arch;
  bool latency_blame_enabled = false;
};



//***************************************************************************
// end namespaces
//***************************************************************************

}
}

#endif
