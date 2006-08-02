// -*-Mode: C++;-*-
// $Id$

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

//****************************************************************************
//
// File:
//   $Source$
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

#include <errno.h>

//*************************** User Include Files *****************************

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

long
ToLong(const char* str)
{
  long value = 0;
  DIAG_ASSERT((str && str[0] != '\0'), "StrUtil::ToLong: empty string!");
  
  errno = 0;
  char* endptr = NULL;
  value = strtol(str, &endptr, 0);
  if (errno || (endptr && strlen(endptr) > 0)) {
    std::string msg = "StrUtil::ToLong: Cannot convert `" + std::string(str) 
      + "'";
    if (errno) { // not always set
      msg += ". ";
      msg += strerror(errno);
    }
    DIAG_THROW(msg);
  } 
  return value;
}


uint64_t
ToUInt64(const char* str)
{
  uint64_t value = 0;
  DIAG_ASSERT((str && str[0] != '\0'), "StrUtil::ToUInt64: empty string!");
  
  errno = 0;
  char* endptr = NULL;
  value = strtoul(str, &endptr, 0);
  if (errno || (endptr && strlen(endptr) > 0)) {
    std::string msg = "StrUtil::ToUInt64: Cannot convert `" + std::string(str)
      + "'";
    if (errno) { // not always set
      msg += ". ";
      msg += strerror(errno);
    }
    DIAG_THROW(msg);
  } 
  return value;
}


double   
ToDbl(const char* str)
{
  double value = 0;
  DIAG_ASSERT((str && str[0] != '\0'), "StrUtil::ToDbl: empty string!");
  
  errno = 0;
  char* endptr = NULL;
  value = strtod(str, &endptr);
  if (errno || (endptr && strlen(endptr) > 0)) {
    std::string msg = "StrUtil::ToDbl: Cannot convert `" + std::string(str)
      + "'";
    if (errno) { // not always set
      msg += ". ";
      msg += strerror(errno);
    }
    DIAG_THROW(msg);
  } 
  return value;
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static char buf[32];

std::string
toStr(int x, int base)
{
  const char* format = NULL;

  switch (base) {
  case 10:
    format = "%d";
    break;
    
  default:
    DIAG_DIE(DIAG_UNIMPLEMENTED);
  }
  
  sprintf(buf, format, x);
  return std::string(buf);
}


std::string
toStr(unsigned x, int base)
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
    DIAG_DIE(DIAG_UNIMPLEMENTED);
  }
  
  sprintf(buf, format, x);
  return std::string(buf);
}


std::string
toStr(int64_t x, int base)
{
  const char* format = NULL;
  
  switch (base) {
  case 10:
    format = "%"PRId64;
    break;
    
  default:
    DIAG_DIE(DIAG_UNIMPLEMENTED);
  }
  
  sprintf(buf, format, x);
  return std::string(buf);
}


std::string
toStr(uint64_t x, int base)
{
  const char* format = NULL;
  
  switch (base) {
  case 10:
    format = "%"PRIu64;
    break;
    
  case 16:
    format = "%#"PRIx64;
    break;

  default:
    DIAG_DIE(DIAG_UNIMPLEMENTED);
  }
  
  sprintf(buf, format, x);
  return std::string(buf);
}


std::string
toStr(double x, const char* format)
{
  //static char buf[19]; // 0xhhhhhhhhhhhhhhhh format
  sprintf(buf, format, x);
  return std::string(buf);
}


//****************************************************************************

} // end of StrUtil namespace
