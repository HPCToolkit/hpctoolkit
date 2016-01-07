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

#include <string>
using std::string;

#include <cstdlib>
#include <cstring>

#include <errno.h>

#define __STDC_FORMAT_MACROS
#include <stdint.h>

//*************************** User Include Files *****************************

#include <include/gcc-attr.h>

#include "StrUtil.hpp"
#include "diagnostics.h"

//************************** Forward Declarations ****************************

//****************************************************************************

//****************************************************************************
// StrUtil
//****************************************************************************

namespace StrUtil {

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void
tokenize_char(const std::string& tokenstr, const char* delim,
	      std::vector<std::string>& tokenvec)
{
  const size_t sz = tokenstr.size();
  for (size_t begp = 0, endp = 0; begp < sz; begp = endp+1) {
    begp = tokenstr.find_first_not_of(delim, begp);
    if (begp == string::npos) {
      break;
    }
    
    endp = tokenstr.find_first_of(delim, begp);
    if (endp == string::npos) {
      endp = sz;
    }
    string x = tokenstr.substr(begp, endp - begp); // [begin, end)
    tokenvec.push_back(x);
  }
}


void
tokenize_str(const std::string& tokenstr, const char* delim,
	     std::vector<std::string>& tokenvec)
{
  const int delimsz = strlen(delim);
  const size_t sz = tokenstr.size();
  
  for (size_t begp = 0, endp = 0; begp < sz; begp = endp + delimsz) {
    endp = tokenstr.find(delim, begp);
    if (endp == string::npos) {
      endp = sz;
    }
    string x = tokenstr.substr(begp, endp - begp); // [begin, end)
    tokenvec.push_back(x);
  }
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

string
join(const std::vector<string>& tokenvec, const char* delim,
     size_t begIdx, size_t endIdx)
{
  string result;

  // N.B.: [begIdx, endIdx)
  for (size_t i = begIdx; i < endIdx; ++i) {
    result += tokenvec[i];
    if (i + 1 < endIdx) { 
      result += delim;
    }
  }

  return result;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

long
toLong(const char* str, unsigned* endidx)
{
  long value = 0;
  DIAG_Assert((str && str[0] != '\0'), "StrUtil::toLong: empty string!");
  
  errno = 0;
  char* endptr = NULL;
  value = strtol(str, &endptr, 0);
  if (endidx) {
    *endidx = (endptr - str) / sizeof(char);
  }
  if (errno || (!endidx && endptr && strlen(endptr) > 0)) {
    string msg = "[StrUtil::toLong] Cannot convert `" + string(str) 
      + "' to integral (long) value";
    if (errno) { // not always set
      msg += string(" (") + strerror(errno) + string(")");
    }
    DIAG_Throw(msg);
  }
  return value;
}


uint64_t
toUInt64(const char* str, unsigned* endidx)
{
  uint64_t value = 0;
  DIAG_Assert((str && str[0] != '\0'), "StrUtil::toUInt64: empty string!");
  
  errno = 0;
  char* endptr = NULL;
  value = strtoull(str, &endptr, 0);
  if (endidx) {
    *endidx = (endptr - str) / sizeof(char);
  }
  if (errno || (!endidx && endptr && strlen(endptr) > 0)) {
    string msg = "[StrUtil::toUInt64] Cannot convert `" + string(str)
      + "' to integral (uint64_t) value";
    if (errno) { // not always set
      msg += string(" (") + strerror(errno) + string(")");
    }
    DIAG_Throw(msg);
  }
  return value;
}


double   
toDbl(const char* str, unsigned* endidx)
{
  double value = 0;
  DIAG_Assert((str && str[0] != '\0'), "StrUtil::toDbl: empty string!");
  
  errno = 0;
  char* endptr = NULL;
  value = strtod(str, &endptr);
  if (endidx) {
    *endidx = (endptr - str) / sizeof(char);
  }
  if (errno || (!endidx && endptr && strlen(endptr) > 0)) {
    string msg = "[StrUtil::toDbl] Cannot convert `" + string(str)
      + "' to real (double) value.";
    if (errno) { // not always set
      msg += string(" (") + strerror(errno) + string(")");
    }
    DIAG_Throw(msg);
  }
  return value;
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static char buf[32];

string
toStr(const int x, int base)
{
  const char* format = NULL;

  switch (base) {
  case 10:
    format = "%d";
    break;
    
  default:
    DIAG_Die(DIAG_Unimplemented);
  }
  
  sprintf(buf, format, x);
  return string(buf);
}


string
toStr(const unsigned x, int base)
{
  const char* format = NULL;

  switch (base) {
  case 10:
    //int numSz = (x == 0) ? 1 : (int) log10((double)l); // no log16...
    //stringSize = 2 + numSz;
    format = "%u";
    break;
    
  case 16:
    //int numSz = (x == 0) ? 1 : (int) log10((double)l);
    //stringSize = 4 + numSz; 
    format = "%#x";
    break;

  default:
    DIAG_Die(DIAG_Unimplemented);
  }
  
  sprintf(buf, format, x);
  return string(buf);
}


string
toStr(const int64_t x, int base)
{
  const char* format = NULL;
  
  switch (base) {
  case 10:
    format = "%" PRId64;
    break;
    
  default:
    DIAG_Die(DIAG_Unimplemented);
  }
  
  sprintf(buf, format, x);
  return string(buf);
}


string
toStr(const uint64_t x, int base)
{
  const char* format = NULL;
  
  switch (base) {
  case 10:
    format = "%" PRIu64;
    break;
    
  case 16:
    format = "%#" PRIx64;
    break;

  default:
    DIAG_Die(DIAG_Unimplemented);
  }
  
  sprintf(buf, format, x);
  return string(buf);
}


string
toStr(const void* x, int GCC_ATTR_UNUSED base)
{
  sprintf(buf, "%p", x);
  return string(buf);
}


string
toStr(const double x, const char* format)
{
  //static char buf[19]; // 0xhhhhhhhhhhhhhhhh format
  sprintf(buf, format, x);
  return string(buf);
}


//****************************************************************************

} // end of StrUtil namespace
