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
#include <iomanip>

#include <string>
using std::string;

#include <exception>

//*********************** Xerces Include Files *******************************

//************************* User Include Files *******************************

#include "Args.hpp"

#include <lib/analysis/Flat-SrcCorrelation.hpp>
#include <lib/analysis/Flat-ObjCorrelation.hpp>
#include <lib/analysis/Raw.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

static int
main_srcCorrelation(const Args& args);

static int
main_objCorrelation(const Args& args);

static int
main_rawData(const std::vector<string>& profileFiles);


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

  int ret = 0;

  switch (args.mode) {
    case Args::Mode_SourceCorrelation:
      ret = main_srcCorrelation(args);
      break;
    case Args::Mode_ObjectCorrelation:
      ret = main_objCorrelation(args);
      break;
    case Args::Mode_RawDataDump:
      ret = main_rawData(args.profileFiles);
      break;
    default:
      DIAG_Die("Unhandled case: " << args.mode);
  }

  return ret; 
}


//****************************************************************************
//
//****************************************************************************

static void
makeDerivedMetrics(Prof::Metric::Mgr& metricMgr, const string& metrics);

static int
main_srcCorrelation(const Args& args)
{
  NaN_init();
  RealPathMgr::singleton().searchPaths(args.searchPathStr());

  //-------------------------------------------------------
  // Create metric descriptors
  //-------------------------------------------------------
  Prof::Metric::Mgr metricMgr;
  metricMgr.makeRawMetrics(args.profileFiles);
  makeDerivedMetrics(metricMgr, args.txt_metrics);

  //-------------------------------------------------------
  // Correlate metrics with program structure and Generate output
  //-------------------------------------------------------
  Prof::Struct::Tree structure("", new Prof::Struct::Root(""));

  Analysis::Flat::Driver driver(args, metricMgr, structure);
  int ret = driver.run();

  return ret;
}


static int
main_objCorrelation(const Args& args)
{
  std::ostream& os = std::cout;
  
  for (int i = 0; i < args.profileFiles.size(); ++i) {
    const std::string& fnm = args.profileFiles[i];

    // 0. Generate nice header
    os << std::setfill('=') << std::setw(77) << "=" << std::endl;
    os << fnm << std::endl;
    os << std::setfill('=') << std::setw(77) << "=" << std::endl;

    // 1. Create metric descriptors
    Prof::Metric::Mgr metricMgr;
    metricMgr.makeRawMetrics(fnm, 
			     false/*isunit_ev*/, 
			     args.obj_metricsAsPercents);
    
    // 2. Correlate
    Analysis::Flat::correlateWithObject(metricMgr, os,
					args.obj_showSourceCode,
					args.obj_procGlobs,
					args.obj_procThreshold);
  }
  return 0;
}


static int
main_rawData(const std::vector<string>& profileFiles)
{
  std::ostream& os = std::cout;

  for (int i = 0; i < profileFiles.size(); ++i) {
    const char* fnm = profileFiles[i].c_str();

    // generate nice header
    os << std::setfill('=') << std::setw(77) << "=" << std::endl;
    os << fnm << std::endl;
    os << std::setfill('=') << std::setw(77) << "=" << std::endl;

    Analysis::Raw::writeAsText(fnm); // pass os FIXME
  }
  return 0;
}

//****************************************************************************

static void
makeDerivedMetrics(Prof::Metric::Mgr& metricMgr, const string& metrics)
{
  if (metrics.empty()) {
    return;
  }

  DIAG_Assert(metrics == "sum" || metrics == "sum-only", DIAG_UnexpectedInput);

  metricMgr.makeSummaryMetrics();
  
  if (metrics == "sum-only") {
    for (uint i = 0; i < metricMgr.size(); i++) {
      PerfMetric* m = metricMgr.metric(i);
      FilePerfMetric* mm = dynamic_cast<FilePerfMetric*>(m);
      if (mm) {
	mm->Display(false);
	mm->SortBy(false);
      }
    }

    for (uint i = 0; i < metricMgr.size(); i++) {
      PerfMetric* m = metricMgr.metric(i);
      ComputedPerfMetric* mm = dynamic_cast<ComputedPerfMetric*>(m);
      if (mm) {
	mm->SortBy(true);
	break;
      }
    }
  }
}
