// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include <string>
using std::string;

#include <cstdlib> // for 'free'

#include "../common/lean/hpctoolkit_demangle.h"
#include "../common/ProcNameMgr.hpp"

#include "BinUtils.hpp"

//****************************************************************************

namespace BinUtil {

// 'canonicalizeProcName': If 'name' is non-empty, uses 'demangleProcName'
// to attempt to demangle it.  If there is an error in demangling,
// return 'name'; otherwise return the demangled version.
string
canonicalizeProcName(const std::string& name, ProcNameMgr* procNameMgr)
{
  if (name.empty()) {
    return name;
  }

  string bestname = demangleProcName(name.c_str());
  if (procNameMgr) {
    bestname = procNameMgr->canonicalize(bestname);
  }

  return bestname;
}


// Returns the demangled function name (if possible) or the original name.
string
demangleProcName(const std::string& name)
{
  string result = name;

  char *str = hpctoolkit_demangle(name.c_str());

  if (str != NULL) {
    result = str;
    free(str);
  }

  return result;
}

} // namespace BinUtil
