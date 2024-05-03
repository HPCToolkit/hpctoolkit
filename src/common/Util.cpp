// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>

#include <string>
using std::string;

#include <algorithm>
#include <typeinfo>

#include <cstring> // strlen()

#include <dirent.h> // scandir()

//*************************** User Include Files ****************************

#include "lean/gcc-attr.h"

#include "Util.hpp"

#include "lean/hpcio.h"
#include "lean/hpcfmt.h"
#include "lean/hpcrun-fmt.h"
#include "lean/hpcrunflat-fmt.h"
#include "lean/formats/cctdb.h"
#include "lean/formats/metadb.h"
#include "lean/formats/profiledb.h"
#include "lean/formats/tracedb.h"

#include "PathFindMgr.hpp"
#include "PathReplacementMgr.hpp"
#include "diagnostics.h"
#include "dictionary.h"
#include "realpath.h"
#include "IOUtil.hpp"

#define DEBUG_DEMAND_STRUCT  0
#define TMP_BUFFER_LEN 1024

//***************************************************************************
//
//***************************************************************************

namespace Analysis {
namespace Util {

Analysis::Util::ProfType_t
getProfileType(const std::string& filenm)
{
  static const int bufSZ = 32;
  char buf[bufSZ] = { '\0' };

  std::istream* is = IOUtil::OpenIStream(filenm.c_str());
  is->read(buf, bufSZ);
  IOUtil::CloseStream(is);

  ProfType_t ty = ProfType_NULL;
  if (strncmp(buf, HPCRUN_FMT_Magic, HPCRUN_FMT_MagicLen) == 0) {
    ty = ProfType_Callpath;
  }
  else if (strncmp(buf, HPCMETRICDB_FMT_Magic, HPCMETRICDB_FMT_MagicLen) == 0) {
    ty = ProfType_CallpathMetricDB;
  }
  else if (strncmp(buf, HPCTRACE_FMT_Magic, HPCTRACE_FMT_MagicLen) == 0) {
    ty = ProfType_CallpathTrace;
  }
  else if (strncmp(buf, HPCRUNFLAT_FMT_Magic, HPCRUNFLAT_FMT_MagicLen) == 0) {
    ty = ProfType_Flat;
  }else if(fmt_profiledb_check(buf, nullptr) != fmt_version_invalid){
    ty = ProfType_ProfileDB;
  }else if(fmt_cctdb_check(buf, nullptr) != fmt_version_invalid){
    ty = ProfType_CctDB;
  }else if(fmt_tracedb_check(buf, nullptr) != fmt_version_invalid){
    ty = ProfType_TraceDB;
  }else if(fmt_metadb_check(buf, nullptr) != fmt_version_invalid){
    ty = ProfType_MetaDB;
  }


  return ty;
}

} // end of Util namespace
} // end of Analysis namespace

//***************************************************************************
//
//***************************************************************************

namespace Analysis {
namespace Util {

OutputOption_t option = Print_All;

} // end of Util namespace
} // end of Analysis namespace


//****************************************************************************
