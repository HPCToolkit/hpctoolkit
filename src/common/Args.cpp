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

#include <limits.h> /* for 'PATH_MAX' */

#include <unistd.h> /* for getcwd() */

//*************************** User Include Files ****************************

#include "Args.hpp"

#include "diagnostics.h"
#include "FileUtil.hpp"
#include "realpath.h"


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
  db_makeMetricDB   = false;
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
    for (unsigned int i = 0; i < searchPathTpls.size(); ++i) {
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
  for (unsigned int i = 0; i < searchPathTpls.size(); ++i) {
    path += string(":") + searchPathTpls[i].first;
  }
  return path;
}


} // namespace Analysis


//***************************************************************************
