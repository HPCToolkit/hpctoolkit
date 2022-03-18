// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "Util.hpp"

#include <lib/prof/pms-format.h>
#include <lib/prof/cms-format.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcrunflat-fmt.h>
#include <lib/prof-lean/tracedb.h>

#include <lib/support/PathFindMgr.hpp>
#include <lib/support/PathReplacementMgr.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/dictionary.h>
#include <lib/support/realpath.h>
#include <lib/support/IOUtil.hpp>

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
  }else if(filenm.find(".sparse-db") != std::string::npos){ //YUMENG: is->read didn't work, may need to FIX later
    ty = ProfType_SparseDBtmp;
  }else if(strncmp(buf, HPCPROFILESPARSE_FMT_Magic, HPCPROFILESPARSE_FMT_MagicLen) == 0){ 
    ty = ProfType_SparseDBthread;
  }else if(strncmp(buf, HPCCCTSPARSE_FMT_Magic, HPCCCTSPARSE_FMT_MagicLen) == 0){ 
    ty = ProfType_SparseDBcct;
  }else if(strncmp(buf, HPCTRACEDB_FMT_Magic, HPCTRACEDB_FMT_MagicLen) == 0){ 
    ty = ProfType_TraceDB;
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
