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
//    ProfileReader.C
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
#include <sstream>
#include <list>
#include <vector>
#include <algorithm>

//*************************** User Include Files ****************************

#include "ProfileReader.h"
#include "PCProfile.h"
#include "DCPIProfile.h"
#include <lib/xml/xml.h>

//*************************** Forward Declarations ***************************

using namespace xml;

using std::cerr;
using std::endl;
using std::hex;
using std::dec;

struct lt_PCProfileMetric
{
  // return true if m1 < m2; false otherwise
  bool operator()(const PCProfileMetric* m1, const PCProfileMetric* m2) const
  { return (strcmp(m1->GetName(), m2->GetName()) < 0); }
};

bool 
MetricNamesAreUnique(PCProfileMetricSet& mset);


//****************************************************************************
//  public: ReadProfileFile: 
//****************************************************************************

// Note: 'ReadProfileFile' guarantees that every 'PCProfileMetric' in the
// 'PCProfile' will have identical return values for both
// 'GetTxtStart' and 'GetTxtSz'.

PCProfile*
ProfileReader::ReadProfileFile(const char* profFile /* FIXME: type */)
{

  // Text or binary!!

  std::ifstream pFile(profFile);
  if ( !pFile.is_open() || pFile.fail()) {
    cerr << "Error opening file `" << profFile << "'" << endl;
    return NULL;
  }

  PCProfile* profData = NULL;

  // depending on file type, choose the correct reader
  profData = dynamic_cast<PCProfile*>(ReadProfileFile_DCPICat(pFile));

  if (!profData) {
    cerr << "Error reading file `" << profFile << "'" << endl;
  }

  pFile.close();
  return profData;
}


//****************************************************************************
//  private: DEC/Alpha/OSF1
//****************************************************************************

#ifdef ALPHA_64
// system dependent headers, funcs...
#endif

// 'ReadProfileFile_DCPICat' reads 'dcpicat' output.  
DCPIProfile*
ProfileReader::ReadProfileFile_DCPICat(std::istream& pFile)
{
  String str;
  const int bufSz = 128; 
  char buf[bufSz]; // holds single tokens guaranteed not to be too big

  DCPIProfile* profData = NULL;  
  std::stringstream hdr; // header info
  String profiledFile;   // name of profiled file
  Addr txtStart = 0;     // starting address of text segment
  Addr txtSz    = 0;     // byte-size of text section
  
  // ------------------------------------------------------------------------
  // Read header
  // ------------------------------------------------------------------------  
  
  // 'name' - name of profiled binary
  str = GetLine(pFile); hdr << (const char*)str << "\n";
  pFile.seekg(-(int)str.Length()-1, std::ios::cur); // reposition to reread
  pFile >> buf >> buf >> std::ws;              // read 'name' token and value
  profiledFile = buf;
    
  // 'image' - date and id information for profiled binary
  str = GetLine(pFile); hdr << (const char*)str << "\n";
  
  // 'label' (optional) - label for this profile file
  if (pFile.peek() == 'l') {
    str = GetLine(pFile); hdr << (const char*)str << "\n";
  }
  
  // 'path' - path for the profiled binary
  //  note: while the 'dcpicat' man-page does not indicate this,
  //   multiple 'path' entries are sometimes given.
  while ( (!pFile.eof() && !pFile.fail()) && pFile.peek() == 'p' ) {
    str = GetLine(pFile); hdr << (const char*)str << "\n";
  }  
  
  // 'epoch' - date the profile was made
  str = GetLine(pFile); hdr << (const char*)str << "\n";

  // 'platform' - name of machine on which the profile was made
  str = GetLine(pFile); hdr << (const char*)str << "\n";

  // 'text_start' - starting address of text section of profiled binary (hex)
  str = GetLine(pFile); hdr << (const char*)str << "\n";
  pFile.seekg(-(int)str.Length()-1, std::ios::cur); // reposition to reread
  pFile >> buf >> std::ws; Skip(pFile, "0x");  // read 'text_start' token...
  pFile >> hex >> txtStart >> dec >> std::ws;  // read 'text_start' value    

  // 'text_size' - byte-size of the text section (hex) 
  str = GetLine(pFile); hdr << (const char*)str << "\n";
  pFile.seekg(-(int)str.Length()-1, std::ios::cur); // reposition to reread
  pFile >> buf >> std::ws; Skip(pFile, "0x");  // read 'text_size' token...
  pFile >> hex >> txtSz >> dec >> std::ws;     // read 'text_size' value

  if (pFile.eof() || pFile.fail()) { return NULL; /* error */ }

  // 'event' - one line for each event type in the profile
  //   Format: 'event W x:X:Y:Z'
  //   (The following adapted from dcpicat man page.)
  //   'W' - sum of this event's count for all pc values recorded in profile
  //   'x' - (only ProfileMe) profileme sample set (attriute and trap bits)
  //   'X' - event name
  //         (or for ProfileMe) the counter name appended to the sample set
  //   'Y' - sampling period used to sample this event (one such event
  //         occurrance is recorded every Y occurrances of the event.)
  //   'Z' - 10000 times the fraction of the time the event was being
  //         monitored.  (When multiple events are being multiplexed onto 
  //         the same hardware counter, "Z" will be less than 10000.
  //
  //  see 'dcpiprofileme' and 'dcpicat' man pages.

  // Create 'PCProfileMetric' from 'event' and collect them all into a queue
  unsigned int metricCount = 0;
  PCProfileMetricList mlist;
  DCPIProfileMetric* curMetric;
  String X;
  PCProfileDatum W, Y, Z;
  bool invalidMetric = false;
  while ( (!pFile.eof() && !pFile.fail()) && pFile.peek() == 'e' ) {

    pFile >> buf;  // read 'event' token (above: line begins with 'e')
    pFile >> W >> std::ws;          // read 'W' (total count)
    
    X = GetLine(pFile, ':');        // read 'x' or 'X' (event name)
    if ( !isdigit(pFile.peek()) ) { // read 'X'
      X += ":" + GetLine(pFile, ':');
    }

    pFile >> Y; Skip(pFile, ":");   // read 'Y' (sampling period) and ':'
    pFile >> Z >> std::ws;          // read 'Z' (sampling time)
    
    curMetric = new DCPIProfileMetric(X);
    curMetric->SetTxtStart(txtStart);
    curMetric->SetTxtSz(txtSz);
    curMetric->SetTotalCount(W);
    curMetric->SetName(X);
    curMetric->SetPeriod(Y);
    if (! curMetric->GetDCPIDesc().IsValid()) {
      invalidMetric = true;
    }

    mlist.push_back(curMetric);
    metricCount++;
  }
  
  // Create 'PCProfile' object and add all events
  profData = new DCPIProfile(metricCount);
  profData->SetHdrInfo( (hdr.str()).c_str() );
  profData->SetProfiledFile(profiledFile);
  
  while (!mlist.empty()) {
    PCProfileMetric* m = mlist.front();
    mlist.pop_front();
    profData->AddMetric(m);
  }
  
  if (invalidMetric || pFile.eof() || pFile.fail()) { 
    goto DCPICat_CleanupAfterError; 
  }
  
  // Check for duplicate 'event' names.  'event' names should be
  // unique, but we have seen at least one instance where it seems
  // duplicates were generated...
  MetricNamesAreUnique(*profData); // Print warning if necessary
  
  // ------------------------------------------------------------------------
  // Read sampling information
  // ------------------------------------------------------------------------  

  // There are 1 PC-column + n count-columns where n is the 'event'
  // number; the columns are in the same order as the 'event' listing.
  // PC's are in increasing address order.  Skipped PC's have '0' for
  // all counts.  Because we are dealing with a non-VLIW architecture,
  // we do not need to use ISA::ConvertPCToOpPC when creating the
  // [pc->count] map.

  // Note: add a dummy block to prevent compiler warnings/errors
  // about the above 'goto' crossing the initialization of 'pcCount'
  {
    unsigned long pcCount = 0; // number of PC entries
    PCProfileDatum profileDatum;
    Addr PC;
    pFile >> std::ws;   
    while ( (!pFile.eof() && !pFile.fail()) ) {
      Skip(pFile, "0x");         // eat up '0x' prefix
      pFile >> hex >> PC >> dec; // read 'PC' value, non VLIW instruction

      profData->AddPC(PC); // there is some non-zero data at PC
      for (unsigned long i = 0; i < metricCount; i++) {
	pFile >> profileDatum;
	const PCProfileMetric* m = profData->GetMetric(i);
	const_cast<PCProfileMetric*>(m)->Insert(PC, profileDatum);
      }
      
      pFile >> std::ws;
      pcCount++;
    }
  }

  if (pFile.fail()) { goto DCPICat_CleanupAfterError; }

  // ------------------------------------------------------------------------
  // Done! 
  // ------------------------------------------------------------------------  
  return profData;

 DCPICat_CleanupAfterError:
  delete profData;
  return NULL;
}

// Check to see if there are duplicates in 'mlist' while preserving order.
bool MetricNamesAreUnique(PCProfileMetricSet& mset)
{
  bool STATUS = true;
  
  if (mset.GetSz() > 1) {
    // Collect all metrics (pointers) into a temporary vector and sort it
    PCProfileMetricVec mvec(mset.GetSz());

    PCProfileMetricSetIterator setIt(mset);
    for (suint i = 0; setIt.IsValid(); ++setIt, ++i) { 
      mvec[i] = setIt.Current();
    }
    
    std::sort(mvec.begin(), mvec.end(), lt_PCProfileMetric()); // ascending
    
    // Check for duplicates.  'mvec.size' is guaranteed to be >= 2
    PCProfileMetric* cur1, *cur2 = NULL;
    PCProfileMetricVecIt it = mvec.begin();
    do {
      cur1 = *it;
      cur2 = *(++it); // increment
      if (strcmp(cur1->GetName(), cur2->GetName()) == 0) {
	STATUS = false;
	cerr << "Warning: Duplicate metric names exist: '"
	     << cur1->GetName() << "'." << endl;
      }
    } while (it+1 != mvec.end());
  
    // Remove all metric pointers from 'mvec' to ensure they are not deleted
    for (it = mvec.begin(); it != mvec.end(); ++it) { *it = NULL; }
  }
  
  return STATUS;
}


//****************************************************************************
//  private: SGI/MIPS/IRIX
//****************************************************************************

#ifdef MIPS_64
// system dependent headers, funcs...
#endif

#ifdef MIPS_32
// system dependent headers, funcs...
#endif


//****************************************************************************
//  private: Sun/SPARC/SunOS
//****************************************************************************

#ifdef SPARC_64
// system dependent headers, funcs...
#endif

#ifdef SPARC_32
// system dependent headers, funcs...
#endif

