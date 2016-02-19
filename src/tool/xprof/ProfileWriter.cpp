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
using std::ostream;
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <fstream>
#include <map>

#include <string>
using std::string;

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "ProfileWriter.hpp"
#include "PCProfile.hpp"
#include "DerivedProfile.hpp"

#include <lib/binutils/LM.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/SrcFile.hpp>

//*************************** Forward Declarations ***************************

const char* UNKNOWN = "<unknown>";

//*************************** Forward Declarations ***************************

// LineToPCProfileVecMap
typedef std::map<SrcFile::ln, PCProfileVec*>   LineToPCProfileVecMap;
typedef LineToPCProfileVecMap::iterator   LineToPCProfileVecMapIt;
typedef LineToPCProfileVecMap::value_type LineToPCProfileVecMapVal;

void 
DumpFuncLineMap(ostream& os, LineToPCProfileVecMap& map, 
		DerivedProfile* profData, 
		const string& func, const string& file);
void 
ClearFuncLineMap(LineToPCProfileVecMap& map);

//****************************************************************************
// 
//****************************************************************************

void
ProfileWriter::WriteProfile(std::ostream& os, DerivedProfile* profData,
			    binutils::LM* lm)
{  
  // Sanity check
  if (profData->GetNumMetrics() == 0) {
    return; // We must have at least one metric
  }
  
  const PCProfile* rawprofData = profData->GetPCProfile();
  ISA* isa = rawprofData->GetISA();
  isa->attach(); // ensure longevity
  
  // ------------------------------------------------------------------------
  // Dump header info
  // ------------------------------------------------------------------------  
  ProfileWriter::DumpProfileHeader(os);

  const string& profiledFile = rawprofData->GetProfiledFile();

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
      os << " displayName";      WriteAttrStr(os, m->GetName());
      os << " nativeName";       WriteAttrStr(os, m->GetNativeName());
      os << " period";           WriteAttrNum(os, m->GetPeriod());
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
  string theFunc, theFile; 
  LineToPCProfileVecMap funcLineMap;

  // Iterate over PC values in the profile
  for (PCProfile_PCIterator it(*rawprofData); it.IsValid(); ++it) {
    
    VMA oppc = it.Current(); // an 'operation pc'
    ushort opIndex;
    VMA pc = isa->convertOpVMAToVMA(oppc, opIndex);
    
    // --------------------------------------------------
    // 1. Attempt to find symbolic information
    // --------------------------------------------------
    string func, file;
    SrcFile::ln line;
    lm->findSrcCodeInfo(pc, opIndex, func, file, line);
    func = GetBestFuncName(func);
    
    // Bad line info: cannot fix and cannot report; advance iteration
    if (!SrcFile::isValid(line)) {
      continue;
    }    
    
    // Bad function name: cannot fix
    if (func.empty()) { func = UNKNOWN; }
    
    // Bad file name: try to fix
    if (file.empty()) {
      if (!theFile.empty() && func == theFunc && func != UNKNOWN) { 
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
    LineToPCProfileVecMapIt it1 = funcLineMap.find(line);
    if (it1 == funcLineMap.end()) {
      // Initialize the vector and insert into the map
      vec = new PCProfileVec(profData->GetNumMetrics());
      funcLineMap.insert(LineToPCProfileVecMapVal(line, vec));
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
  
  isa->detach();
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
		DerivedProfile* profData, 
		const string& func, const string& file)
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
    SrcFile::ln srcLn = (*it).first;
    PCProfileVec* vec = (*it).second;
    
    os << I[3] << "<S b"; WriteAttrNum(os, srcLn);
    os << " id";          WriteAttrNum(os, 0); // was: SrcLineX.id
    os << ">\n";
    for (unsigned int i = 0; i < vec->GetSz(); ++i) {
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

void ClearFuncLineMap(LineToPCProfileVecMap& map)
{
  LineToPCProfileVecMapIt it;
  for (it = map.begin(); it != map.end(); ++it) {
    delete (*it).second;
  }
  map.clear();
}

//***************************************************************************
