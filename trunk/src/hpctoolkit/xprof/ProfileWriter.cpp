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
//    PCProfileUtils.C
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

#include "PCProfileUtils.h"
#include "PCProfile.h"

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
typedef std::map<SrcLineX*, PCProfileVec*, lt_SrcLineX>::iterator
  LineToPCProfileVecMapIt;
typedef std::map<SrcLineX*, PCProfileVec*>::value_type
  LineToPCProfileVecMapVal;

void DumpFuncLineMap(ostream& ofile, LineToPCProfileVecMap& map,
		     PCProfile* profData,
		     const char* func, const char* file);
void ClearFuncLineMap(LineToPCProfileVecMap& map);

//****************************************************************************
// Combine 'PCProfile' with symbolic info and dump
//****************************************************************************

void
DumpWithSymbolicInfo(ostream& os, PCProfile* profData, LoadModuleInfo* modInfo)
{
  const char* profiledFile = profData->GetProfiledFile();
  PCProfileMetric* m = profData->GetMetric(0);
  const Addr txtStart = m->GetTxtStart();       // All metrics guaranteed to  
  const Addr txtEnd = txtStart + m->GetTxtSz(); //   have identical values.
  const unsigned long numMetrics = profData->GetMetricVecSz();

  // ------------------------------------------------------------------------
  // Dump header info
  // ------------------------------------------------------------------------  
  DumpProfileHeader(os);
  os << "<PROFILE version=\"3.0\">\n";
  os << "<PROFILEHDR>\n" << profData->GetHdrInfo() << "</PROFILEHDR>\n";
  os << "<PROFILEPARAMS>\n";
  {
    os << "<TARGET name"; WriteAttrStr(os, profiledFile); os << "/>\n";
    os << "<METRICS>\n";
    for (suint i = 0; i < numMetrics; i++) {
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

	// We want to iterate only over PC values for which counts are
	// recorded.  Note: Every metric in 'profData' has entries for
	// the same pc values.
	if (profData->GetMetric(0)->Find(pc) == PCProfileDatum_NIL) {
	  continue;
	}

	// --------------------------------------------------
	// Attempt to find symbolic information
	// --------------------------------------------------
	String func, file;
	SrcLineX srcLn; 
	modInfo->GetSymbolicInfo(pc, inst->GetOpIndex(), func, file, srcLn);
	
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
	LineToPCProfileVecMapIt it1 = funcLineMap.find(&srcLn);
	if (it1 != funcLineMap.end()) { // found -- update counts
	  PCProfileVec* vec = (*it1).second;
	  for (unsigned long i = 0; i < numMetrics; i++) {
	    m = profData->GetMetric(i);
	    (*vec)[i] += m->Find(pc);
	    BriefAssertion(m->Find(pc) != PCProfileDatum_NIL);
	  }     
	} else { // no entry found -- assign counts
	  PCProfileVec* vec = new PCProfileVec(numMetrics);
	  for (unsigned long i = 0; i < numMetrics; i++) {
	    m = profData->GetMetric(i);
	    (*vec)[i] = m->Find(pc);
	    BriefAssertion(m->Find(pc) != PCProfileDatum_NIL);
	  }
	  
	  // Don't keep 'vec' if all pc counts are 0.
	  if (!vec->IsZeroed()) {
	    funcLineMap.insert(LineToPCProfileVecMapVal(new SrcLineX(srcLn),
							vec));
	  } else {
	    delete vec;
	  }
	}
      }

      // --------------------------------------------------
      // Dump the contents of 'funcLineMap'
      // --------------------------------------------------
      if (funcLineMap.size() > 0 && !theFunc.Empty() && !theFile.Empty()) {
	DumpFuncLineMap(os, funcLineMap, profData, theFunc, theFile);
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
  DumpProfileFooter(os);
}

//****************************************************************************
// 
//****************************************************************************

const char *PROFILEdtd =
#include <lib/xml/PROFILE.dtd.h>

void DumpProfileHeader(ostream& os)
{
  os << "<?xml version=\"1.0\"?>" << endl;
  os << "<!DOCTYPE PROFILE [\n" << PROFILEdtd << "]>" << endl;
  os.flush();
}

void DumpProfileFooter(ostream& os)
{
  /* nothing to do */
}

// Output should be in increasing order by line number.
void DumpFuncLineMap(ostream& os, LineToPCProfileVecMap& map,
		     PCProfile* profData, const char* func, const char* file)
{
  // 'func' will be different than the last call; 'file' may be the same
  static String theFile; // initializes to empty string

  if (strcmp(theFile, file) != 0) {
    // close and begin new file
    // (but don't close if this is the first time (theFile is empty)...
  }

  static const char* I[] = { "", // Indent levels (0 - 5)
			     "  ",
			     "    ",
			     "      ",
			     "        ",
			     "          " };
  
  os << I[1] << "<F n";  WriteAttrStr(os, file); os << ">\n";
  
  // Output 'func'
  os << I[2] << "<P n";  WriteAttrStr(os, func); os << ">\n";
    
  LineToPCProfileVecMapIt it;
  for (it = map.begin(); it != map.end(); ++it) {
    SrcLineX* srcLn = (*it).first;
    PCProfileVec* vec = (*it).second;
    
    os << I[3] << "<S b"; WriteAttrNum(os, srcLn->GetSrcLine());
    os << " id";     WriteAttrNum(os, srcLn->GetSrcLineX());
    os << ">\n";
    for (suint i = 0; i < vec->GetSz(); ++i) {
      if ( (*vec)[i] != 0 ) {
	PCProfileMetric* e = profData->GetMetric(i);
	double v = (double)(*vec)[i] * (double)e->GetPeriod();
	
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
