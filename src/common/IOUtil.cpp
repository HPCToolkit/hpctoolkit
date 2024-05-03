// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
  char buf[bufSz];
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
