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

//****************************************************************************
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
// Author:
//   Nathan Tallent
//
//****************************************************************************

//************************** System Include Files ****************************

#include <iostream>
#include <fstream>

//*************************** User Include Files *****************************

#include "IOUtil.hpp"
#include "diagnostics.h"

//************************** Forward Declarations ****************************

//****************************************************************************

//****************************************************************************
// IOUtil
//****************************************************************************

namespace IOUtil {


std::istream*
OpenIStream(const char* filenm)
{
  if (!filenm || filenm[0] == '\0') {
    // Use cin
    return &std::cin;
  } 
  else {
    std::ifstream* ifs = new std::ifstream;
    try {
      OpenIFile(*ifs, filenm);
      return ifs;
    }
    catch (const Diagnostics::Exception& /*ex*/) {
      delete ifs;
      throw;
    }
  }
}


std::ostream*
OpenOStream(const char* filenm)
{
  if (!filenm || filenm[0] == '\0') {
    // Use cout
    return &std::cout;
  } 
  else {
    std::ofstream* ofs = new std::ofstream;
    try {
      OpenOFile(*ofs, filenm);
      return ofs;
    }
    catch (Diagnostics::Exception& /*ex*/) {
      delete ofs;
      throw;
    }
  }
}


void
CloseStream(std::istream* s)
{
  if (s != &std::cin) {
    delete s;
  }
}


void
CloseStream(std::ostream* s)
{
  if (s != &std::cout) {
    delete s;
  }
}


void
CloseStream(std::iostream* s)
{
  if (s != &std::cin && s != &std::cout) {
    delete s;
  }
}


void
OpenIFile(std::ifstream& fs, const char* filenm)
{
  using namespace std;

  fs.open(filenm, ios::in);
  if (!fs.is_open() || fs.fail()) {
    DIAG_Throw("Cannot open file '" << filenm << "'");
  }
}


void 
OpenOFile(std::ofstream& fs, const char* filenm)
{
  using namespace std;

  fs.open(filenm, ios::out | ios::trunc);
  if (!fs.is_open() || fs.fail()) {
    DIAG_Throw("Cannot open file '" << filenm << "'");
  }
}


void
CloseFile(std::fstream& fs)
{
  fs.close();
}


std::string 
Get(std::istream& is, char end)
{
  static const int bufSz = 256;
  static char buf[bufSz];
  std::string str;
  
  while ( (!is.eof() && !is.fail()) && is.peek() != end) {
    is.get(buf, bufSz, end);
    str += buf;
  }  

  return str;
}


std::string 
GetLine(std::istream& is, char end)
{
  std::string str = Get(is, end);
  char c; is.get(c);  // eat up 'end'
  return str;
}


bool 
Skip(std::istream& is, const char* s)
{
  DIAG_Assert(s, DIAG_UnexpectedInput);
  char c;
  for (int i = 0; s[i] != '\0'; i++) {
    is.get(c);
    if (c != s[i]) { return false; }
  }
  return true;
}


//****************************************************************************

} // end of IOUtil namespace

