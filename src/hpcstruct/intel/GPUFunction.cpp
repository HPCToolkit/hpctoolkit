// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "GPUFunction.hpp"



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {



//***************************************************************************
// interface operations
//***************************************************************************

void GPUFunction::setEntry(Block *entry) {
  _region = entry->region();
  _entry = entry;
}



//***************************************************************************
// end namespaces
//***************************************************************************

}
}
