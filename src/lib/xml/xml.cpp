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

#include <fstream>

#include <string>
using std::string;

#include <cstring>

//*************************** User Include Files ****************************

#include "xml.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

using namespace xml;

const string xml::SPC   = " ";   // space
const string xml::eleB  = "<";   // element begin, initial
const string xml::eleBf = "</";  // element begin, final
const string xml::eleE  = ">";   // element end, normal
const string xml::eleEc = "/>";  // element end, compact: <.../>
const string xml::attB  = "=\""; // attribute value begin
const string xml::attE  = "\"";  // attribute value end

//****************************************************************************
// Read
//****************************************************************************

// Reads from 'attB' to and including 'attE'.
bool
xml::ReadAttrStr(std::istream& is, string& s, int flags)
{
  bool STATE = true; // false indicates an error
  is >> std::ws;
  STATE &= IOUtil::Skip(is, "=");  is >> std::ws;
  STATE &= IOUtil::Skip(is, "\""); is >> std::ws;
  s = IOUtil::Get(is, '"');
  if (flags & UNESC_TRUE) { s = UnEscapeStr(s); }
  STATE &= IOUtil::Skip(is, "\""); is >> std::ws;
  return STATE;
}


//****************************************************************************
// Write
//****************************************************************************

// Writes attribute value, beginning with 'attB' and ending with 'attE'.
bool
xml::WriteAttrStr(std::ostream& os, const char* s, int flags)
{
  string str = ((flags & ESC_TRUE) ? EscapeStr(s) : s);
  os << attB << str << attE;
  return (!os.fail());
}


//****************************************************************************
//
//****************************************************************************

// 'EscapeStr' and 'UnEscapeStr': Returns the string with all
// necessary characters (un)escaped; will not modify 'str'

namespace xml {
  static string
  substitute(const char* str, const string* fromStrs, const string* toStrs);

  static const int numSubs = 4; // number of substitutes
  static const string RegStrs[]  = {"<",    ">",    "&",     "\""};
  static const string EscStrs[]  = {"&lt;", "&gt;", "&amp;", "&quot;"};
}


string
xml::EscapeStr(const char* str)
{
  return substitute(str, RegStrs, EscStrs);
}


string
xml::UnEscapeStr(const char* str)
{
  return substitute(str, EscStrs, RegStrs);
}


static string
xml::substitute(const char* str, const string* fromStrs, const string* toStrs)
{
  static string newStr = string("", 512);

  string retStr = str;
  if (!str) { return retStr; }

  // Iterate over 'str' and substitute patterns
  newStr = "";
  int strLn = strlen(str);
  for (int i = 0; str[i] != '\0'; /* */) {

    // Attempt to find a pattern for substitution at this position
    int curSub = 0, curSubLn = 0;
    for (/*curSub = 0*/; curSub < numSubs; curSub++) {
      curSubLn = fromStrs[curSub].length();
      if ((strLn-i >= curSubLn) &&
	  (strncmp(str+i, fromStrs[curSub].c_str(), curSubLn) == 0)) {
	break; // only one substitution possible per position
      }
    }

    // Substitute or copy current position; Adjust iteration to
    // inspect next character. (resizes if necessary)
    if (curSub < numSubs) { // we found a string to substitute
      newStr += toStrs[curSub]; i += curSubLn;
    } else {
      newStr += str[i];         i++;
    }
  }
  retStr = newStr;

  return retStr;
}

//****************************************************************************
//
//****************************************************************************
