// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef BANAL_GPU_GPU_FUNCTION_H
#define BANAL_GPU_GPU_FUNCTION_H

//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <CFG.h>



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {



//***************************************************************************
// type declarations
//***************************************************************************

class GPUFunction : public ParseAPI::Function {
 public:
  GPUFunction(Address addr, std::string name, CodeObject * obj,
    CodeRegion * region, InstructionSource * isource) :
    Function(addr, name, obj, region, isource) {
    _cache_valid = true;
  }

  virtual ~GPUFunction() {}

  void setEntry(Block *entry);
};



//***************************************************************************
// end namespaces
//***************************************************************************

}
}

#endif
