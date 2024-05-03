// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef BANAL_GPU_GPU_CFG_FACTORY_H
#define BANAL_GPU_GPU_CFG_FACTORY_H

//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <CFGFactory.h>



//***************************************************************************
// TBB includes
//***************************************************************************

#include <tbb/concurrent_hash_map.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "GPUBlock.hpp"
#include "GPUCFG.hpp"



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {



//***************************************************************************
// type declarations
//***************************************************************************


class GPUCFGFactory : public CFGFactory {
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



//***************************************************************************
// end namespaces
//***************************************************************************

}
}

#endif
