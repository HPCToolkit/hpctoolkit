// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
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

#ifndef support_StrUtil_hpp
#define support_StrUtil_hpp

//************************** System Include Files ****************************

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <inttypes.h>

//*************************** User Include Files *****************************

//************************** Forward Declarations ****************************

//****************************************************************************
// StrUtil
//****************************************************************************

namespace StrUtil {

// --------------------------------------------------------------------------
// tokenize_char: Given a string of tokens 'tokenstr' delimited by any of the
// *individual characters* in 'delim', extract and place each token
// string in 'tokenvec'.
//
// tokenize_str: Same as tokenize_char, except that the delimiter
// itself is one string (rather than several possible single characters).
//
// --------------------------------------------------------------------------

void
tokenize_char(const std::string& tokenstr, const char* delim,
              std::vector<std::string>& tokenvec);

void
tokenize_str(const std::string& tokenstr, const char* delim,
              std::vector<std::string>& tokenvec);


// --------------------------------------------------------------------------
// join: Given a vector of tokens 'tokenvec' and a delimiter 'delim',
// form a string by placing delim in between every element of
// tokenvec[begIdx ... endIdx).
// --------------------------------------------------------------------------

std::string
join(const std::vector<std::string>& tokenvec, const char* delim,
     size_t begIdx, size_t endIdx);


// --------------------------------------------------------------------------
// string -> numerical types
//
// Comments [FIXME]: see strtol.  If 'endidx' is non-NULL, it is set
// to the index (relative to 'str') of the first non-numerical
// character.  (Thus, the entire string is valid if endidx ==
// length(str).)  If 'endidx' is NULL, an error is raised if any
// 'junk' appears after the proposed string.
//
// --------------------------------------------------------------------------

long
toLong(const char* str, unsigned* endidx = NULL);

inline long
toLong(const std::string& str, unsigned* endidx = NULL)
{
  return toLong(str.c_str(), endidx);
}


uint64_t
toUInt64(const char* str, unsigned* endidx = NULL);

inline uint64_t
toUInt64(const std::string& str, unsigned* endidx = NULL)
{
  return toUInt64(str.c_str(), endidx);
}


double
toDbl(const char* str, unsigned* endidx = NULL);

inline double
toDbl(const std::string& str, unsigned* endidx = NULL)
{
  return toDbl(str.c_str(), endidx);
}


// --------------------------------------------------------------------------
// numerical types -> string
//   base: one of 10, 16
// --------------------------------------------------------------------------

std::string
toStr(const int x, int base = 10);

std::string
toStr(const unsigned x, int base = 10);

std::string
toStr(const int64_t x, int base = 10);

std::string
toStr(const uint64_t x, int base = 10);

std::string
toStr(const void* x, int base = 16);

std::string
toStr(const double x, const char* format = "%.3f");


} // end of StrUtil namespace


#endif // support_StrUtil_hpp
