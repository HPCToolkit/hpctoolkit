// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    main.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <new>

//*************************** User Include Files ****************************

#include "Args.h"
#include "ProfileReader.h"
#include "ProfileWriter.h"
#include "PCProfile.h"
#include "DerivedProfile.h"

#include "DCPIProfile.h"

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/binutils/LoadModuleInfo.h>

//*************************** Forward Declarations ***************************

using std::cerr;
using std::cout;
using std::endl;

PCProfileFilterList*
GetAvailPredefDCPIFilters(DCPIProfile* prof, LoadModule* lm);

void
ListAvailPredefDCPIFilters(DCPIProfile* prof, bool longlist = false);

//****************************************************************************

int
main(int argc, char* argv[])
{
  Args args(argc, argv);
  
  // ------------------------------------------------------------
  // Read 'pcprof', the raw profiling data file
  // ------------------------------------------------------------
  PCProfile* pcprof = NULL;
  try {
    pcprof = ProfileReader::ReadProfileFile(args.profFile /*filetype*/);
    if (!pcprof) { exit(1); }
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading profile!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading profile!\n";
    exit(2);
  }
  
  
  if (args.listAvailableMetrics > 0) {
    // * Assume DCPI mode *
    DCPIProfile* dcpiprof = dynamic_cast<DCPIProfile*>(pcprof);
    ListAvailPredefDCPIFilters(dcpiprof, args.listAvailableMetrics == 2);
    return (0);
  }
  
  // ------------------------------------------------------------
  // Read executable
  // ------------------------------------------------------------
  Executable* exe = NULL;
  try {
    exe = new Executable();
    if (!exe->Open(args.progFile)) { exit(1); } // Error already printed 
    if (!exe->Read()) { exit(1); }              // Error already printed 
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading binary!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading binary!\n";
    exit(2);
  }
  
  // ------------------------------------------------------------
  // Read 'PCToSrcLineXMap', if available
  // ------------------------------------------------------------
  PCToSrcLineXMap* map = NULL;
  try {
    if (!args.pcMapFile.Empty()) {
      map = ReadPCToSrcLineMap(args.pcMapFile);
      if (!map) {
	cerr << "Error reading file `" << args.pcMapFile << "'\n";
	exit(1); 
      }
    }
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading map!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading map!\n";
    exit(2);
  }
  
  LoadModuleInfo modInfo(exe, map);
    
  // ------------------------------------------------------------
  // Construct some metrics, derived in some way from the raw
  // profiling data, that we are interested in.
  // ------------------------------------------------------------
  DerivedProfile* drvdprof = NULL;
  try {
    // * Assume DCPI mode *
    PCProfileFilterList* filtList = NULL;
    if (!args.outputRawMetrics) {
      DCPIProfile* dcpiprof = dynamic_cast<DCPIProfile*>(pcprof);
      filtList = GetAvailPredefDCPIFilters(dcpiprof, modInfo.GetLM());
    }
    
    drvdprof = new DerivedProfile(pcprof, filtList);
    delete filtList;

  } catch (...) {
    cerr << "Error: Exception encountered while constructing derived profile!\n";
    exit(2);
  }
  
  // ------------------------------------------------------------
  // Translate the derived metrics to a PROFILE file
  // ------------------------------------------------------------
  try {
    // * Assume DCPI mode *
    ProfileWriter::WriteProfile(cout, drvdprof, &modInfo);
  } catch (...) {
    cerr << "Error: Exception encountered while translating to PROFILE!\n";
    exit(2);
  }
  
  delete exe;
  delete map;
  delete pcprof;
  delete drvdprof;
  return (0);
}

//****************************************************************************

bool
IsPredefDCPIFilterAvail(DCPIProfile* prof, PredefinedDCPIMetricTable::Entry* e);

// GetAvailPredefDCPIFilters: Return a list of all the filters within
// the DCPI predefined metric table that are available for this
// profile.
PCProfileFilterList*
GetAvailPredefDCPIFilters(DCPIProfile* prof, LoadModule* lm)
{
  PCProfileFilterList* flist = new PCProfileFilterList;
  for (uint i = 0; i < PredefinedDCPIMetricTable::GetSize(); ++i) {
    PredefinedDCPIMetricTable::Entry* e = PredefinedDCPIMetricTable::Index(i);
    if (IsPredefDCPIFilterAvail(prof, e)) {
      flist->push_back(GetPredefinedDCPIFilter(e->name, lm));
    }
  }
  return flist;
}

// ListAvailPredefDCPIFilters: Iterate through the DCPI predefined
// metric table, lising any metrics (filters) that are available for
// this profile.
void
ListAvailPredefDCPIFilters(DCPIProfile* prof, bool longlist)
{
  // When longlist is false, a short listing is printed
  // When longlist is true, a long listing is print (name and description)

  String out;
  for (uint i = 0; i < PredefinedDCPIMetricTable::GetSize(); ++i) {
    PredefinedDCPIMetricTable::Entry* e = PredefinedDCPIMetricTable::Index(i);
    if (IsPredefDCPIFilterAvail(prof, e)) {
      
      if (longlist) {
	out += String("  ") + e->name + ": " + e->description + "\n";
      } else {
	if (!out.Empty()) { out += ":"; }
	out += e->name;
      }
      
    }
  }
  
  if (out.Empty()) {
    cout << "No derived metrics available for this DCPI profile.";
  } else {
    cout << "The following metrics are available for this DCPI profile:\n";
    if (longlist) {
      cout << out;
    } else {
      cout << "  -M " << out << endl;
      cout << "(This line may be edited and passed to xprof.)" << endl;
    }
  }
}

// IsPredefDCPIFilterAvail: Given the DCPIProfile and an entry in the
// DCPI predefined metric table, determine if the corresponding filter
// is available (meaningful) for this profile.
bool 
IsPredefDCPIFilterAvail(DCPIProfile* prof, PredefinedDCPIMetricTable::Entry* e)
{
  DCPIProfile::PMMode pmmode = prof->GetPMMode();
  
  if (e->avail & PredefinedDCPIMetricTable::RM) {

    // The filter may or may not be available (whether in ProfileMe
    // mode or not): must check each metric
    for (PCProfileMetricSetIterator it(*prof); it.IsValid(); ++it) {
      PCProfileMetric* m = it.Current();
      if (strcmp(e->name, m->GetName()) == 0) {
	return true;
      }
    }
    return false;
    
  } else {
    // The filter may be available in any ProfileMe mode or in only
    // one mode.

    bool pmavail = false;
    if (e->avail & PredefinedDCPIMetricTable::PM0) {
      if (pmmode == DCPIProfile::PM0) { pmavail |= true; }
    } 
    if (e->avail & PredefinedDCPIMetricTable::PM1) {
      if (pmmode == DCPIProfile::PM1) { pmavail |= true; }
    }
    if (e->avail & PredefinedDCPIMetricTable::PM2) {
      if (pmmode == DCPIProfile::PM2) { pmavail |= true; }
    }
    if (e->avail & PredefinedDCPIMetricTable::PM3) {
      if (pmmode == DCPIProfile::PM3) { pmavail |= true; }
    }
    return pmavail;
  }
  
}
