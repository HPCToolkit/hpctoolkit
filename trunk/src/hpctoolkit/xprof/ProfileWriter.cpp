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
//    ProfileWriter.C
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
#include <map>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

#include "ProfileWriter.h"
#include "PCProfile.h"
#include "DerivedProfile.h"

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/Section.h>
#include <lib/binutils/Procedure.h>
#include <lib/binutils/Instruction.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/binutils/BinUtils.h>
#include <lib/support/String.h> 
#include <lib/xml/xml.h>

//*************************** Forward Declarations ***************************

using namespace xml;

using std::ostream;
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

const char* UNKNOWN = "<unknown>";

// LineToPCProfileVecMap

struct lt_SrcLineX
{
  // return true if s1 < s2; false otherwise
  bool operator()(const SrcLineX* s1, const SrcLineX* s2) const
  {
    if (s1->GetSrcLine() == s2->GetSrcLine()) {
      return (s1->GetSrcLineX() < s2->GetSrcLineX());
    } else {
      return (s1->GetSrcLine() < s2->GetSrcLine());
    }
  }
};

typedef std::map<SrcLineX*, PCProfileVec*, lt_SrcLineX> LineToPCProfileVecMap;
typedef LineToPCProfileVecMap::iterator   LineToPCProfileVecMapIt;
typedef LineToPCProfileVecMap::value_type LineToPCProfileVecMapVal;

void 
DumpFuncLineMap(ostream& os, LineToPCProfileVecMap& map, 
		DerivedProfile* profData, const char* func, const char* file);
void 
ClearFuncLineMap(LineToPCProfileVecMap& map);

//****************************************************************************
// 
//****************************************************************************

void
ProfileWriter::WriteProfile(std::ostream& os, DerivedProfile* profData,
			    LoadModuleInfo* modInfo)
{  
  const PCProfile* rawprofData = profData->GetPCProfile();
  const ISA* isa = rawprofData->GetISA();

  // ------------------------------------------------------------------------
  // Dump header info
  // ------------------------------------------------------------------------  
  ProfileWriter::DumpProfileHeader(os);

  const char* profiledFile = rawprofData->GetProfiledFile();

  os << "<PROFILE version=\"3.0\">\n";
  os << "<PROFILEHDR>\n" << rawprofData->GetHdrInfo() << "</PROFILEHDR>\n";
  os << "<PROFILEPARAMS>\n";
  {
    os << "<TARGET name"; WriteAttrStr(os, profiledFile); os << "/>\n";
    os << "<METRICS>\n";
    
    DerivedProfile_MetricIterator it(*profData);
    for (uint i = 0; it.IsValid(); ++it, ++i) {
      DerivedProfileMetric* m = it.Current();
      os << "<METRIC shortName"; WriteAttrNum(os, i);
      os << " nativeName";       WriteAttrNum(os, m->GetNativeName());
      os << " period";           WriteAttrNum(os, m->GetPeriod());
      os << " displayName";      WriteAttrNum(os, m->GetName());
      // os << " count_total";   WriteAttrNum(os, m->GetTotalCount());
      os << "/>\n";
    }
    os << "</METRICS>\n";
    os << "</PROFILEPARAMS>\n";
  }

  // ------------------------------------------------------------------------
  // Iterate through the PC values in the profile, collect symbolic
  // information on a source line basis, and output the results
  // ------------------------------------------------------------------------  

  os << "<PROFILESCOPETREE>\n";
  os << "<PGM n"; WriteAttrStr(os, profiledFile); os << ">\n";

  // 'funcLineMap' maps a line number to a 'PCProfileVec'.  It should
  //   contain information only for the current function, 'theFunc'.
  String theFunc, theFile; 
  LineToPCProfileVecMap funcLineMap;

  // Iterate over PC values in the profile
  for (PCProfile_PCIterator it(*rawprofData); it.IsValid(); ++it) {
    
    Addr oppc = it.Current(); // an 'operation pc'
    ushort opIndex;
    Addr pc = isa->ConvertOpPCToPC(oppc, opIndex);
    
    Instruction* inst = modInfo->GetLM()->GetInst(pc, opIndex);
    BriefAssertion(inst && "Internal Error: Cannot find instruction!");
    
    // --------------------------------------------------
    // 1. Attempt to find symbolic information
    // --------------------------------------------------
    String func, file;
    SrcLineX srcLn; 
    modInfo->GetSymbolicInfo(pc, opIndex, func, file, srcLn);
    
    // Bad line info: cannot fix and cannot report; advance iteration
    if ( !IsValidLine(srcLn.GetSrcLine()) ) {
      continue;
    }    
    
    // Bad function name: cannot fix
    if (func.Empty()) { func = UNKNOWN; }
    
    // Bad file name: try to fix
    if (file.Empty()) {
      if (!theFile.Empty() && func == theFunc && func != UNKNOWN) { 
	file = theFile;	// valid replacement
      } else {
	file = UNKNOWN;
      }
    }
    
    // --------------------------------------------------
    // 2. If we are starting a new function, dump 'funcLineMap'
    // --------------------------------------------------
    if (func != theFunc) {
      DumpFuncLineMap(os, funcLineMap, profData, theFunc, theFile);
      ClearFuncLineMap(funcLineMap); // clears map and memory
    }

    // --------------------------------------------------
    // 3. Update 'funcLineMap' for each derived metric
    // --------------------------------------------------
    PCProfileVec* vec = NULL;
    LineToPCProfileVecMapIt it1 = funcLineMap.find(&srcLn);
    if (it1 == funcLineMap.end()) {
      // Initialize the vector and insert into the map
      vec = new PCProfileVec(profData->GetNumMetrics());
      funcLineMap.insert(LineToPCProfileVecMapVal(new SrcLineX(srcLn), vec));
    } else {
      vec = (*it1).second;
    }
    
    // Update vector counts for each derived metric.  Note that we
    // only update when the current pc applies to the derived metric.
    DerivedProfile_MetricIterator dmIt(*profData);
    for (uint i = 0; dmIt.IsValid(); ++dmIt, ++i) {
      DerivedProfileMetric* dm = dmIt.Current();
      if (dm->FindPC(pc, opIndex)) {
	PCProfileMetricSetIterator mIt( *(dm->GetMetricSet()) );
	for ( ; mIt.IsValid(); ++mIt) {
	  PCProfileMetric* m = mIt.Current();
	  (*vec)[i] += m->Find(pc, opIndex);
	}
      }
    }
    
    if (theFunc != func) { theFunc = func; }
    if (theFile != file) { theFile = file; }
  }
  
  // --------------------------------------------------
  // 4. Dump 'funcLineMap', if it contains info for the last function
  // --------------------------------------------------
  if (funcLineMap.size() > 0) {
    DumpFuncLineMap(os, funcLineMap, profData, theFunc, theFile);
    ClearFuncLineMap(funcLineMap); // clears map and memory
  }
  
  os << "</PGM>\n";
  os << "</PROFILESCOPETREE>\n";
  
  // ------------------------------------------------------------------------
  // Dump footer
  // ------------------------------------------------------------------------  
  os << "</PROFILE>\n"; 
  ProfileWriter::DumpProfileFooter(os);    
}

//****************************************************************************
// Combine 'PCProfile' with symbolic info and dump
//****************************************************************************

void //FIXME
DumpFuncLineMap_old(ostream& ofile, LineToPCProfileVecMap& map,
		PCProfile* profData, const char* func, const char* file);

void
DumpAsPROFILE(ostream& os, PCProfile* profData, LoadModuleInfo* modInfo)
{
  const char* profiledFile = profData->GetProfiledFile();
  const PCProfileMetric* m = profData->GetMetric(0);
  const Addr txtStart = m->GetTxtStart();       // All metrics guaranteed to  
  const Addr txtEnd = txtStart + m->GetTxtSz(); //   have identical values.
  const ulong numMetrics = profData->GetSz();

  // ------------------------------------------------------------------------
  // Dump header info
  // ------------------------------------------------------------------------  
  ProfileWriter::DumpProfileHeader(os);
  os << "<PROFILE version=\"3.0\">\n";
  os << "<PROFILEHDR>\n" << profData->GetHdrInfo() << "</PROFILEHDR>\n";
  os << "<PROFILEPARAMS>\n";
  {
    os << "<TARGET name"; WriteAttrStr(os, profiledFile); os << "/>\n";
    os << "<METRICS>\n";
    for (ulong i = 0; i < numMetrics; i++) {
      m = profData->GetMetric(i);
      os << "<METRIC shortName"; WriteAttrNum(os, i);
      os << " nativeName";       WriteAttrNum(os, m->GetName());
      os << " period";           WriteAttrNum(os, m->GetPeriod());
      // os << " count_total";   WriteAttrNum(os, m->GetTotalCount());
      os << "/>\n";
    }
    os << "</METRICS>\n";
    os << "</PROFILEPARAMS>\n";
  }

  // ------------------------------------------------------------------------
  // Iterate through the PC values of the text section, collect
  //   symbolic information on a source line basis, and output the results
  // ------------------------------------------------------------------------  

  os << "<PROFILESCOPETREE>\n";
  os << "<PGM n"; WriteAttrStr(os, profiledFile); os << ">\n";
  
  // 'funcLineMap' maps a line number to a 'PCProfileVec'.  It should
  //   contain information only for the current function, 'theFunc'.
  LineToPCProfileVecMap funcLineMap;

  for (LoadModuleSectionIterator it(*modInfo->GetLM()); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->GetType() != Section::Text) { continue; }

    // We have a 'TextSection'.  Iterate over procedures.
    TextSection* tsec = dynamic_cast<TextSection*>(sec);
    for (TextSectionProcedureIterator it(*tsec); it.IsValid(); ++it) {
      Procedure* p = it.Current();
      String pName = GetBestFuncName(p->GetName());

      // We have a 'Procedure'.  Iterate over PC values
      String theFunc = pName, theFile; 
      for (ProcedureInstructionIterator it(*p); it.IsValid(); ++it) {
	Instruction* inst = it.Current();
	Addr pc = inst->GetPC();
	ushort opIndex = inst->GetOpIndex();

	// We want to iterate only over PC values for which counts are
	// recorded.  
	long foundMetric;
	if ( (foundMetric = profData->DataExists(pc, opIndex)) < 0) {
	  continue;
	}
	
	// --------------------------------------------------
	// Attempt to find symbolic information
	// --------------------------------------------------
	String func, file;
	SrcLineX srcLn; 
	modInfo->GetSymbolicInfo(pc, opIndex, func, file, srcLn);
	
	if (theFile.Empty() && !file.Empty()) { theFile = file; }
	
	// Bad line numbers: cannot fix; advance iteration
	if (!IsValidLine(srcLn.GetSrcLine())) {
	  continue; // No useful symbolic information; advance iteration
	}

	// Bad/Different func name: ignore for now and use 'theFunc' (FIXME)
	// Bad file name: ignore and use 'theFile' (FIXME)
	  
	// --------------------------------------------------
	// Update 'funcLineMap'
	// --------------------------------------------------    
	PCProfileVec* vec = NULL;
	LineToPCProfileVecMapIt it1 = funcLineMap.find(&srcLn);
	if (it1 == funcLineMap.end()) {
	  // Initialize the vector and insert into the map
	  vec = new PCProfileVec(numMetrics);
	  funcLineMap.insert(LineToPCProfileVecMapVal(new SrcLineX(srcLn), vec));
	} else {
	  vec = (*it1).second;
	}

	// Update vector counts
	for (ulong i = (ulong)foundMetric; i < numMetrics; i++) {
	  m = profData->GetMetric(i);
	  (*vec)[i] += m->Find(pc, opIndex); // add
	}	
      }

      // --------------------------------------------------
      // Dump the contents of 'funcLineMap'
      // --------------------------------------------------
      if (!theFunc.Empty() && !theFile.Empty()) {
	DumpFuncLineMap_old(os, funcLineMap, profData, theFunc, theFile);
      }
      ClearFuncLineMap(funcLineMap); // clears map and memory
      
    }
  }
  
  os << "</PGM>\n";
  os << "</PROFILESCOPETREE>\n";

  // ------------------------------------------------------------------------
  // Dump footer
  // ------------------------------------------------------------------------  

  os << "</PROFILE>\n"; 
  ProfileWriter::DumpProfileFooter(os);
}

//****************************************************************************
// 
//****************************************************************************

const char *PROFILEdtd =
#include <lib/xml/PROFILE.dtd.h>

void 
ProfileWriter::DumpProfileHeader(ostream& os)
{
  os << "<?xml version=\"1.0\"?>" << endl;
  os << "<!DOCTYPE PROFILE [\n" << PROFILEdtd << "]>" << endl;
  os.flush();
}

void 
ProfileWriter::DumpProfileFooter(ostream& os)
{
  /* nothing to do */
}

// Output should be in increasing order by line number.
void 
DumpFuncLineMap(ostream& os, LineToPCProfileVecMap& map, 
		DerivedProfile* profData, const char* func, const char* file)
{
  static const char* I[] = { "", // Indent levels (0 - 5)
			     "  ",
			     "    ",
			     "      ",
			     "        ",
			     "          " };

  if (map.size() == 0) { return; }
  
  os << I[1] << "<F n";  WriteAttrStr(os, file); os << ">\n";
  os << I[2] << "<P n";  WriteAttrStr(os, func); os << ">\n";
    
  LineToPCProfileVecMapIt it;
  for (it = map.begin(); it != map.end(); ++it) {
    SrcLineX* srcLn = (*it).first;
    PCProfileVec* vec = (*it).second;
    
    os << I[3] << "<S b"; WriteAttrNum(os, srcLn->GetSrcLine());
    os << " id";          WriteAttrNum(os, srcLn->GetSrcLineX());
    os << ">\n";
    for (suint i = 0; i < vec->GetSz(); ++i) {
      if ( (*vec)[i] != 0 ) {
	const DerivedProfileMetric* dm = profData->GetMetric(i);
	double v = (double)(*vec)[i] * (double)dm->GetPeriod();
	
	os << I[4] << "<M n"; WriteAttrNum(os, i);
	os << " v";   WriteAttrNum(os, v);
	os << "/>\n";
      }
    }
    os << I[3] << "</S>" << endl;
  }

  os << I[2] << "</P>" << endl;
  os << I[1] << "</F>\n";
}

// Output should be in increasing order by line number.
void 
DumpFuncLineMap_old(ostream& os, LineToPCProfileVecMap& map,
		    PCProfile* profData, const char* func, const char* file)
{
  static const char* I[] = { "", // Indent levels (0 - 5)
			     "  ",
			     "    ",
			     "      ",
			     "        ",
			     "          " };

  if (map.size() == 0) { return; }
  
  os << I[1] << "<F n";  WriteAttrStr(os, file); os << ">\n";
  os << I[2] << "<P n";  WriteAttrStr(os, func); os << ">\n";
    
  LineToPCProfileVecMapIt it;
  for (it = map.begin(); it != map.end(); ++it) {
    SrcLineX* srcLn = (*it).first;
    PCProfileVec* vec = (*it).second;
    
    os << I[3] << "<S b"; WriteAttrNum(os, srcLn->GetSrcLine());
    os << " id";          WriteAttrNum(os, srcLn->GetSrcLineX());
    os << ">\n";
    for (suint i = 0; i < vec->GetSz(); ++i) {
      if ( (*vec)[i] != 0 ) {
	const PCProfileMetric* m = profData->GetMetric(i);
	double v = (double)(*vec)[i] * (double)m->GetPeriod();
	
	os << I[4] << "<M n"; WriteAttrNum(os, i);
	os << " v";   WriteAttrNum(os, v);
	os << "/>\n";
      }
    }
    os << I[3] << "</S>" << endl;
  }

  os << I[2] << "</P>" << endl;
  os << I[1] << "</F>\n";
}


void ClearFuncLineMap(LineToPCProfileVecMap& map)
{
  LineToPCProfileVecMapIt it;
  for (it = map.begin(); it != map.end(); ++it) {
    delete (*it).first;
    delete (*it).second;
  }
  map.clear();
}

//***************************************************************************
