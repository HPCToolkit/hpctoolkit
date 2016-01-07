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
// Copyright ((c)) 2002-2016, Rice University
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

#include <limits.h> /* for 'PATH_MAX' */

#include <unistd.h> /* for getcwd() */

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/realpath.h>


//*************************** Forward Declarations **************************

//***************************************************************************
// Args
//***************************************************************************

namespace Analysis {


Args::Args()
{
  Ctor();
}


void
Args::Ctor()
{
  // -------------------------------------------------------
  // Correlation arguments
  // -------------------------------------------------------

  doNormalizeTy = true;

  prof_metrics = Analysis::Args::MetricFlg_NULL;

  profflat_computeFinalMetricValues = true;

  // -------------------------------------------------------
  // Output arguments
  // -------------------------------------------------------

  out_db_experiment = Analysis_OUT_DB_EXPERIMENT;
  out_db_csv        = "";
  db_dir            = Analysis_DB_DIR_pfx "-" Analysis_DB_DIR_nm;
  db_copySrcFiles   = true;
  out_db_config     = "";
  db_makeMetricDB   = true;
  db_addStructId    = false;

  out_txt           = Analysis_OUT_TXT;
  txt_summary       = TxtSum_NULL;
  txt_srcAnnotation = false;

  // laks: disable redundancy elimination by default
  remove_redundancy = false;
}


Args::~Args()
{
}


string
Args::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void
Args::dump(std::ostream& os) const
{
  os << "db_dir= " << db_dir << std::endl;
  os << "out_db_experiment= " << out_db_experiment << std::endl;
  os << "out_db_csv= " << out_db_csv << std::endl;
  os << "out_txt= " << out_txt << std::endl;
}


void
Args::ddump() const
{
  dump(std::cerr);
}


} // namespace Analysis


//***************************************************************************

namespace Analysis {


void
Args::normalizeSearchPaths()
{
  char cwd[PATH_MAX+1];
  getcwd(cwd, PATH_MAX);

  for (PathTupleVec::iterator it = searchPathTpls.begin();
       it != searchPathTpls.end(); /* */) {
    string& x = it->first; // current path
    PathTupleVec::iterator x_it = it;

    ++it; // advance iterator
    
    if (chdir(x.c_str()) == 0) {
      char norm_x[PATH_MAX+1];
      getcwd(norm_x, PATH_MAX);
      x = norm_x; // replace x with norm_x
    }
    else {
      DIAG_Msg(1, "Discarding search path: " << x);
      searchPathTpls.erase(x_it);
    }
    chdir(cwd);
  }
  
  if (searchPathTpls.size() > 0) {
    DIAG_Msg(2, "search paths:");
    for (uint i = 0; i < searchPathTpls.size(); ++i) {
      DIAG_Msg(2, "  " << searchPathTpls[i].first);
    }
  }
}


void
Args::makeDatabaseDir()
{
  // prepare output directory (N.B.: chooses a unique name!)
  string dir = db_dir; // make copy
  std::pair<string, bool> ret =
    FileUtil::mkdirUnique(dir); // N.B.: exits on failure...
  db_dir = RealPath(ret.first.c_str());
}


std::string
Args::searchPathStr() const
{
  string path = ".";
  for (uint i = 0; i < searchPathTpls.size(); ++i) {
    path += string(":") + searchPathTpls[i].first;
  }
  return path;
}


} // namespace Analysis


//***************************************************************************


