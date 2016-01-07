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
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <fstream>
#include <sstream>
#include <list>
#include <vector>
#include <algorithm>

#include <string>
using std::string;

#include <cstdlib>
#include <cstring>

//*************************** User Include Files ****************************

#include "ProfileReader.hpp"
#include "PCProfile.hpp"
#include "DCPIProfile.hpp"

#include <lib/isa/ISA.hpp>
#include <lib/isa/AlphaISA.hpp>

#include <lib/support/IOUtil.hpp>

//*************************** Forward Declarations ***************************

struct lt_PCProfileMetric
{
  // return true if m1 < m2; false otherwise
  bool operator()(const PCProfileMetric* m1, const PCProfileMetric* m2) const
  { return (strcmp(m1->GetName().c_str(), m2->GetName().c_str()) < 0); }
};

static bool 
MetricNamesAreUnique(PCProfileMetricSet& mset);


static DCPIProfile::PMMode
DeterminePMMode(const char* pmcounter);

inline DCPIProfile::PMMode
DeterminePMMode(const string& pmcounter) 
{ return DeterminePMMode(pmcounter.c_str()); }


static const char*
GetSecondSubstring(const char* str);

inline const char*
GetSecondSubstring(const string& str)
{ return GetSecondSubstring(str.c_str()); }

//****************************************************************************
//  public: ReadProfileFile: 
//****************************************************************************

// ReadProfileFile: Given the name of a profile file 'profFile',
// creates a 'PCProfile' from the raw profile data.  If 'profFile' is
// NULL or the empty string, reads from cin.
//
// Note: Guarantees that every 'PCProfileMetric' in the 'PCProfile'
// will have identical return values for both 'GetTxtStart' and
// 'GetTxtSz'.
PCProfile*
ProfileReader::ReadProfileFile(const char* profFile /* FIXME: type */)
{
  PCProfile* profData = NULL;
  std::istream* is = NULL;
  bool usingStdin = (!profFile || profFile[0] == '\0');
  
  // ------------------------------------------------------------
  // Open an input stream and determine its type
  // ------------------------------------------------------------  
  if (!usingStdin) {
    
    // Attempt to open as a file
    std::ifstream* pFile = new std::ifstream(profFile);
    is = pFile;
    if ( !pFile->is_open() || pFile->fail()) {
      cerr << "Error opening file `" << profFile << "'" << endl;
      goto ReadProfileFile_CleanupAfterError;
    }
    
  } else {
    // Read from cin
    is = &(std::cin);
  }
  
  // Figure out type (text, binary?)

  // ------------------------------------------------------------
  // Read the data
  // ------------------------------------------------------------  

  // depending on file type, choose the correct reader
  profData = dynamic_cast<PCProfile*>(ReadProfileFile_DCPICat(*is));
  
  if (!profData) {
    if (!usingStdin) {
      cerr << "Error reading file `" << profFile << "'" << endl;
    } else {
      cerr << "Error reading from stdin" << endl;
    }
  }

  // ------------------------------------------------------------
  // Cleanup
  // ------------------------------------------------------------  
 ReadProfileFile_CleanupAfterError:
  if (!usingStdin) {
    delete is;
  }
  
  return profData;
}


//****************************************************************************
//  private: DEC/Alpha/OSF1
//****************************************************************************

// 'ReadProfileFile_DCPICat' reads 'dcpicat' output.  
DCPIProfile*
ProfileReader::ReadProfileFile_DCPICat(std::istream& is)
{
  string str;
  const int bufSz = 128; 
  char buf[bufSz]; // holds single tokens guaranteed not to be too big
  
  ISA* isa = new AlphaISA(); // we are going to reference count this
  DCPIProfile* profData = NULL;  
  std::stringstream hdr;   // header info
  string profiledFile;     // name of profiled file
  string profiledFilePath; // path to profiled file
  VMA txtStart = 0;       // starting address of text segment
  VMA txtSz    = 0;       // byte-size of text section
  
  // ------------------------------------------------------------
  // Read header
  // ------------------------------------------------------------  

  hdr << std::showbase;  

  // 'name' - name of profiled binary
  str = IOUtil::GetLine(is); hdr << str << "\n";
  profiledFile = GetSecondSubstring(str);  // value of 'name'
  
  // 'image' - date and id information for profiled binary
  str = IOUtil::GetLine(is); hdr << str << "\n";
  
  // 'label' (optional) - label for this profile file
  if (is.peek() == 'l') {
    str = IOUtil::GetLine(is); hdr << str << "\n";
  }
  
  // 'path' - path for the profiled binary
  //  note: while the 'dcpicat' man-page does not indicate this,
  //  multiple 'path' entries are sometimes given.
  while ( (!is.eof() && !is.fail()) && is.peek() == 'p' ) {
    str = IOUtil::GetLine(is); hdr << str << "\n";
    if (profiledFilePath.empty()) {
      profiledFilePath = GetSecondSubstring(str);  // value of 'path'
    }
  }  
  
  // 'epoch' - date the profile was made
  str = IOUtil::GetLine(is); hdr << str << "\n";
  
  // 'platform' - name of machine on which the profile was made
  str = IOUtil::GetLine(is); hdr << str << "\n";
  
  // 'text_start' - starting address of text section of profiled binary (hex)
  str = IOUtil::Get(is, '0'); hdr << str;  // read until 0x
  IOUtil::Skip(is, "0x");                  // eat up '0x' prefix
  is >> hex >> txtStart >> dec >> std::ws; // value of 'text_start'
  hdr << hex << txtStart << dec << "\n";
  
  // 'text_size' - byte-size of the text section (hex) 
  str = IOUtil::Get(is, '0'); hdr << str; // read until 0x
  IOUtil::Skip(is, "0x");                 // eat up '0x' prefix
  is >> hex >> txtSz >> dec >> std::ws;   // value of 'text_size'
  hdr << hex << txtSz << dec << "\n";
  
  if (is.eof() || is.fail()) { return NULL; /* error */ }
  
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
  PCProfileMetricList mlist;
  DCPIProfileMetric* curMetric;
  DCPIProfile::PMMode pmmode = DCPIProfile::PM_NONE;
  
  string X;
  PCProfileDatum W, Y, Z;
  bool invalidMetric = false;
  unsigned int metricCount = 0;

  while ( (!is.eof() && !is.fail()) && is.peek() == 'e' ) {

    is >> buf;  // read 'event' token (above: line begins with 'e')
    is >> W >> std::ws;          // read 'W' (total count)
    
    X = IOUtil::GetLine(is, ':'); // read 'x' or 'X' (event name)
    if ( !isdigit(is.peek()) ) {  // read 'X' (for ProfileMe)
      string pmcounter = IOUtil::GetLine(is, ':');
      pmmode = DeterminePMMode(pmcounter);
      if (pmmode == DCPIProfile::PM_NONE) { invalidMetric = true; }
      X += ":" + pmcounter;
    }

    is >> Y; IOUtil::Skip(is, ":"); // read 'Y' (sampling period) and ':'
    is >> Z >> std::ws;             // read 'Z' (sampling time)
    
    curMetric = new DCPIProfileMetric(isa, X);
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
  profData = new DCPIProfile(isa, metricCount);
  isa->detach(); // Remove our reference
  profData->SetHdrInfo( (hdr.str()).c_str() );
  if (!profiledFilePath.empty()) {
    profData->SetProfiledFile(profiledFilePath);
  } 
  else { 
    profData->SetProfiledFile(profiledFile);
  }
  profData->SetPMMode(pmmode);
  
  while (!mlist.empty()) {
    PCProfileMetric* m = mlist.front();
    mlist.pop_front();
    profData->AddMetric(m);
  }
  
  if (invalidMetric || is.eof() || is.fail()) { 
    goto DCPICat_CleanupAfterError; 
  }
  
  // Check for duplicate 'event' names.  'event' names should be
  // unique, but we have seen at least one instance where it seems
  // duplicates were generated...
  MetricNamesAreUnique(*profData); // Print warning if necessary
  
  // ------------------------------------------------------------
  // Read sampling information
  // ------------------------------------------------------------  

  // There are 1 PC-column + n count-columns where n is the 'event'
  // number; the columns are in the same order as the 'event' listing.
  // PC's are in increasing address order.  Skipped PC's have '0' for
  // all counts.  Because we are dealing with a non-VLIW architecture,
  // the 'opIndex' argument to 'AddPC(...)' and 'Insert(...)' is always 0.

  // Note: add a dummy block to prevent compiler warnings/errors
  // about the above 'goto' crossing the initialization of 'pcCount'
  {
    unsigned long pcCount = 0; // number of PC entries
    PCProfileDatum profileDatum;
    VMA PC;
    is >> std::ws;
    while ( (!is.eof() && !is.fail()) ) {
      IOUtil::Skip(is, "0x"); // eat up '0x' prefix
      is >> hex >> PC >> dec; // read 'PC' value, non VLIW instruction

      profData->AddPC(PC, 0); // there is some non-zero data at PC
      for (unsigned long i = 0; i < metricCount; i++) {
	is >> profileDatum;
	const PCProfileMetric* m = profData->GetMetric(i);
	const_cast<PCProfileMetric*>(m)->Insert(PC, 0, profileDatum);
      }
      
      is >> std::ws;
      pcCount++;
    }
  }

  if (is.fail()) { goto DCPICat_CleanupAfterError; }

  // ------------------------------------------------------------
  // Done! 
  // ------------------------------------------------------------  
  return profData;

 DCPICat_CleanupAfterError:
  delete profData;
  return NULL;
}

// Check to see if there are duplicates in 'mlist' while preserving order.
static bool 
MetricNamesAreUnique(PCProfileMetricSet& mset)
{
  bool STATUS = true;
  
  if (mset.GetSz() > 1) {
    // Collect all metrics (pointers) into a temporary vector and sort it
    PCProfileMetricVec mvec(mset.GetSz());

    PCProfileMetricSetIterator setIt(mset);
    for (unsigned int i = 0; setIt.IsValid(); ++setIt, ++i) { 
      mvec[i] = setIt.Current();
    }
    
    std::sort(mvec.begin(), mvec.end(), lt_PCProfileMetric()); // ascending
    
    // Check for duplicates.  'mvec.size' is guaranteed to be >= 2
    PCProfileMetric* cur1, *cur2 = NULL;
    PCProfileMetricVecIt it = mvec.begin();
    do {
      cur1 = *it;
      cur2 = *(++it); // increment
      if (cur1->GetName() == cur2->GetName()) {
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

// Given a ProfileMe counter name, determine which ProfileMe mode was used
static DCPIProfile::PMMode
DeterminePMMode(const char* pmcounter)
{
  if (strncmp("m0", pmcounter, 2) == 0) {
    return DCPIProfile::PM0;
  } else if (strncmp("m1", pmcounter, 2) == 0) {
    return DCPIProfile::PM1;
  } else if (strncmp("m2", pmcounter, 2) == 0) {
    return DCPIProfile::PM2;
  } else if (strncmp("m3", pmcounter, 2) == 0) {
    return DCPIProfile::PM3;
  }
  return DCPIProfile::PM_NONE;
}

// GetSecondString: Given a string composed of substrings separated by
// whitespace, return a pointer to the beginning of the second
// substring.  Ignores any whitespace at the beginning of the first
// substring. E.g.
//   name                 hydro
//   text_start           0x000120000000
static const char*
GetSecondSubstring(const char* str) 
{
  if (!str) { return NULL; }
  
  const char* ptr = str;
  
  // ignore first bit of whitespace
  while (*ptr != '\0' && isspace(*ptr)) { ptr++; }
  
  // ignore first substring
  while (*ptr != '\0' && !isspace(*ptr)) { ptr++; }
  
  // ignore second bit of whitespace
  while (*ptr != '\0' && isspace(*ptr)) { ptr++; }
  
  return ptr; // beginning of second substring
}

