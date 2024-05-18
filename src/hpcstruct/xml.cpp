// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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

#include "../common/diagnostics.h"

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
  string newStr = string("", 512);

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
