// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "GPUCodeSource.hpp"



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {



//***************************************************************************
// interface operations
//***************************************************************************

GPUCodeSource::GPUCodeSource(
  std::vector<GPUParse::Function *> &functions, Dyninst::SymtabAPI::Symtab *s) {
  for (auto *function : functions) {
    Address address = function->address;
    _hints.push_back(Hint(address, 0, 0, function->name));
  }
}



//***************************************************************************
// end namespaces
//***************************************************************************

}
}
