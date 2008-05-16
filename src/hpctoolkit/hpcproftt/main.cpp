// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

//************************ System Include Files ******************************

#include <iostream>
#include <fstream>

#include <string>
using std::string;

#include <exception>

//*********************** Xerces Include Files *******************************

//************************* User Include Files *******************************

#include "Args.hpp"

#include <lib/analysis/Flat.hpp>

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

//****************************************************************************

int realmain(int argc, char* const* argv);

int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  }
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  }
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }

  return ret;
}


int 
realmain(int argc, char* const* argv) 
{
  Args args(argc, argv);  // exits if error on command line

  NaN_init();

  Analysis::Flat::Driver driver(args);
  //-------------------------------------------------------
  // 1. Initialize metric descriptors
  //-------------------------------------------------------
  driver.makePerfMetricDescs(args.profileFiles);
  
  //-------------------------------------------------------
  // 2. Initialize static program structure
  //-------------------------------------------------------
  PgmScopeTree scopeTree("", new PgmScope(""));
  driver.createProgramStructure(scopeTree); 

  //-------------------------------------------------------
  // 3. Correlate metrics with program structure
  //-------------------------------------------------------
  driver.correlateMetricsWithStructure(scopeTree);
  
  //-------------------------------------------------------
  // 4. Finalize scope tree
  //-------------------------------------------------------
  PruneScopeTreeMetrics(scopeTree.GetRoot(), driver.numMetrics());
  scopeTree.GetRoot()->Freeze();      // disallow further additions to tree 
  scopeTree.CollectCrossReferences(); // collect cross referencing information

  //-------------------------------------------------------
  // 5. Generate Experiment database
  //-------------------------------------------------------

#if 0
  if (args.outFilename_XML != "no") {
    int flg = (args.metrics_computeInteriorValues) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;

    const string& fnm = args.outFilename_XML;
    DIAG_Msg(1, "Writing final scope tree (in XML) to " << fnm);
    string fpath = args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.XML_Dump(scopeTree.GetRoot(), flg, *os);
    IOUtil::CloseStream(os);
  }
#endif

  //-------------------------------------------------------
  // Cleanup
  //-------------------------------------------------------
  ClearPerfDataSrcTable(); 
  
  return 0; 
} 

//****************************************************************************
//
//****************************************************************************
