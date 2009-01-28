// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
//   Nathan Tallent
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
# include <string.h>
# include <errno.h>
#else
# include <cstdlib> // for strtol
# include <cstring>
# include <cerrno> 
using namespace std; // For compatibility with non-std C headers
#endif

#include <algorithm> // for sort

//************************** Open64 Include Files ***************************

//*************************** User Include Files ****************************

#include "CmdLineParser.hpp"

#include "diagnostics.h"
#include "StrUtil.hpp"

//*************************** Forward Declarations ***************************

using std::string;

static string MISSING_SWITCH = "Missing switch after -";
static string UNKNOWN_SWITCH = "Unknown option switch: ";
static string MISSING_ARG = "Missing argument for switch: ";

//*************************** Forward Declarations ***************************

// lt_OptArgDesc: Used to sort CmdLineParser::OptArgDesc
struct lt_OptArgDesc
{
  // return true if x1 < x2; false otherwise
  bool operator()(const CmdLineParser::OptArgDesc& x1, 
		  const CmdLineParser::OptArgDesc& x2) const
  {
    // There are three possibilities, listed in order of preference:
    //   - both long switches are present 
    //   - both short switches are present
    //   - one short and one long switch
    if (x1.swLong && x2.swLong) {
      return (strcmp(x1.swLong, x2.swLong) < 0);
    }
    else if (x1.swShort != 0 && x2.swShort != 0) {
      return (x1.swShort < x2.swShort);
    } 
    else {
      if (x1.swLong && x2.swShort != 0) {
	return (x1.swLong[0] < x2.swShort);
      } 
      else {
	return (x1.swShort < x2.swLong[0]);
      }
    }
  }

private:

};

//****************************************************************************

// isDashDash
static inline bool
isDashDash(const char* str) { return (strcmp(str, "--") == 0); }


// IsSwitch, IsLongSwitch, IsShortSwitch: Assumes str is non-NULL!  Note also
// that the test for short switch is not quite complete and depends on
// testing for a long switch first!
static inline bool
isLongSwitch(const char* str) { return (strncmp(str, "--", 2) == 0); }

static inline bool
isShortSwitch(const char* str) { return (*str == '-'); }

static inline bool
isSwitch(const char* str) { return (isLongSwitch(str) || isShortSwitch(str)); }


// isArg: Verifies that we should interpret 'str' as an argument.
// Should be non-NULL;
static inline bool
isArg(const char* str) { return (!isSwitch(str) && !isDashDash(str)); }


//****************************************************************************

//****************************************************************************
// CmdLineParser
//****************************************************************************

CmdLineParser::OptArgDesc CmdLineParser::OptArgDesc_NULL = 
  CmdLineParser_OptArgDesc_NULL_MACRO;


CmdLineParser::CmdLineParser() 
{
  Ctor();
}

CmdLineParser::CmdLineParser(const OptArgDesc* optArgDescs, 
			     int argc, const char* const argv[])
{
  Ctor();
  parse(optArgDescs, argc, argv);
}

void
CmdLineParser::Ctor() 
{
  // nothing to do
}



CmdLineParser::~CmdLineParser() 
{ 
  Reset();
}


void
CmdLineParser::parse(const OptArgDesc* optArgDescsOrig, 
		     int argc, const char* const argv[])
{ 
  Reset();
  command = argv[0]; // always do first so it will be available after errors
  
  CheckForErrors(optArgDescsOrig);  
  const OptArgDesc* optArgDescs = CreateSortedCopy(optArgDescsOrig);
  
  bool endOfOpts = false;  // are we at end of optional args?
  
  for (int i = 1; i < argc; ++i) {
    const char* str = argv[i];
    
    // -------------------------------------------------------
    // Bypass special option values
    // -------------------------------------------------------
    if (str == NULL || *str == '\0') {
      continue; // should never happen, but we ignore
    }
    
    // A '--' signifies end of optional arguments
    if (isDashDash(str)) {
      endOfOpts = true;
      continue;
    }
    
    if (!endOfOpts && isSwitch(str)) {
      // -------------------------------------------------------
      // An option switch (possibly needing an argument)
      // -------------------------------------------------------
      // Note: The argument may be appended to the switch or it may be
      // the next element of argv.
      
      // 1. Separate switch from any argument embedded within
      SwDesc swdesc = MakeSwitchDesc(str);
      if (swdesc.sw.empty()) {
	throw ParseError(MISSING_SWITCH); // must have been '-'
      }
      
      // 2. Find option descriptor from switch (checks for duplicate matches)
      const OptArgDesc* d = FindOptDesc(optArgDescs, swdesc);
      if (!d) {
	throw ParseError(UNKNOWN_SWITCH + swdesc.sw);
      }
      
      // 3. Find argument for switch (if any) [N.B. may advance iteration!]
      if (d->kind == ARG_NONE) {
	if (!swdesc.arg.empty()) {
	  string msg = "Invalid argument `" + swdesc.arg + "' to switch `" 
	    + swdesc.sw + "'";
	  throw ParseError(msg);
	}
      } 
      else if (d->kind == ARG_REQ || d->kind == ARG_OPT) {
	if (swdesc.arg.empty()) {
	  int nexti = i + 1;
	  if (nexti < argc && argv[nexti]) {
	    const char* nxt = argv[nexti];
	    bool isarg_nxt = 
	      (d->isOptArgFn) ? isArg(nxt) && d->isOptArgFn(nxt) : isArg(nxt);
	    if (isarg_nxt) {
	      swdesc.arg = nxt;
	      i = nexti; // increment iteration
	    }
	  }
	} 
	if (swdesc.arg.empty() && d->kind == ARG_REQ) {
	  throw ParseError(MISSING_ARG + swdesc.sw);
	}
      }
      
      // 4. Add option switch and any argument to map
      AddOption(*d, swdesc);
    }
    else { 
      // -------------------------------------------------------
      // A regular argument
      // -------------------------------------------------------
      arguments.push_back(string(str));
    } 
  } 
  
  delete[] optArgDescs;
}


//****************************************************************************

const string& 
CmdLineParser::getCmd() const
{
  return command;
}


// isOpt:
bool 
CmdLineParser::isOpt(const char swShort) const
{
  string sw(1, swShort);
  return isOpt(sw);
}

bool 
CmdLineParser::isOpt(const char* swLong) const
{
  string sw(swLong);
  return isOpt(sw);
}

bool 
CmdLineParser::isOpt(const string& sw) const
{
  SwitchToArgMap::const_iterator it = switchToArgMap.find(sw);
  return (it != switchToArgMap.end());
}


// isOptArg:
bool 
CmdLineParser::isOptArg(const char swShort) const
{
  string sw(1, swShort);
  return isOptArg(sw);
}

bool 
CmdLineParser::isOptArg(const char* swLong) const
{
  string sw(swLong);
  return isOptArg(sw);
}

bool 
CmdLineParser::isOptArg(const string& sw) const
{
  SwitchToArgMap::const_iterator it = switchToArgMap.find(sw);
  if ((it != switchToArgMap.end()) && ((*it).second != NULL)) {
    return true;
  }
  return false;
}


// getOptArg:
const string&
CmdLineParser::getOptArg(const char swShort) const
{
  string sw(1, swShort);
  return getOptArg(sw);
}

const string&
CmdLineParser::getOptArg(const char* swLong) const
{
  string sw(swLong);
  return getOptArg(sw);
}

const string&
CmdLineParser::getOptArg(const string& sw) const
{
  SwitchToArgMap::const_iterator it = switchToArgMap.find(sw);
  if (it == switchToArgMap.end()) {
    // FIXME: ERROR
  }
  string* arg = (*it).second;
  if (!arg) {
    // FIXME: ERROR
  }
  return *arg;
}


unsigned int 
CmdLineParser::getNumArgs() const
{ 
  return arguments.size(); 
}

const string& 
CmdLineParser::getArg(unsigned int i) const
{
  return arguments[i];
}


//****************************************************************************

long
CmdLineParser::toLong(const string& str)
{
  if (str.empty()) { throw InternalError("ToLong"); }
  
  try {
    return StrUtil::toLong(str);
  }
  catch (const Diagnostics::FatalException& x) {
    throw ParseError(x.what());
  }
}


uint64_t
CmdLineParser::toUInt64(const string& str)
{
  if (str.empty()) { throw InternalError("ToUInt64"); }
  
  try {
    return StrUtil::toUInt64(str);
  }
  catch (const Diagnostics::FatalException& x) {
    throw ParseError(x.what());
  }
}


double   
CmdLineParser::toDbl(const string& str)
{
  if (str.empty()) { throw InternalError("ToDbl"); }
  
  try {
    return StrUtil::toDbl(str);
  }
  catch (const Diagnostics::FatalException& x) {
    throw ParseError(x.what());
  }
}


//****************************************************************************

bool 
CmdLineParser::isOptArg_long(const char* option)
{
  try {
    CmdLineParser::toLong(string(option));
  }
  catch (const CmdLineParser::Exception& x) {
    return false;
  }
  return true;
}


//****************************************************************************

void 
CmdLineParser::dump(std::ostream& os) const
{
  os << "Command: `" << getCmd() << "'" << std::endl;
  
  os << "Switch to Argument map:" << std::endl;
  for (SwitchToArgMap::const_iterator it = switchToArgMap.begin();
       it != switchToArgMap.end(); ++it) {
    const string& sw = (*it).first;
    const string* arg = (*it).second;
    os << "  " << sw << " --> " << ((arg) ? *arg : "<>") << std::endl;
  }
  
  os << "Regular arguments:" << std::endl;
  for (unsigned int i = 0; i < arguments.size(); ++i) {
    os << "  " << arguments[i] << std::endl;
  }
}


void 
CmdLineParser::ddump() const
{
  dump(std::cerr);
}


//****************************************************************************

// Reset: Clear data to prepare for parsing
void
CmdLineParser::Reset()
{
  for (SwitchToArgMap::iterator it = switchToArgMap.begin();
       it != switchToArgMap.end(); ++it) {
    string* arg = (*it).second;
    delete arg;
  }
  switchToArgMap.clear();
  arguments.clear();
}


// CreateSortedCopy: create a sorted NULL-terminated copy of
// 'optArgDescs'.  WARNING: the OptArgDesc objects are bitwise-copied.
const CmdLineParser::OptArgDesc* 
CmdLineParser::CreateSortedCopy(const OptArgDesc* optArgDescs)
{
  // Find the size, not including the NULL-terminator
  unsigned int sz = 0; 
  for (const OptArgDesc* p = optArgDescs; *p != OptArgDesc_NULL; ++p) { ++sz; }
  
  // Make a copy of 'optArgDescs'
  OptArgDesc* copy = new OptArgDesc[sz+1];
  unsigned int i = 0; 
  for (const OptArgDesc* p = optArgDescs; *p != OptArgDesc_NULL; ++p, ++i) {
    copy[i] = *p; // bitwise copy is ok
  }
  copy[sz] = OptArgDesc_NULL; // add the NULL-terminator
  
  // Sort
  if (sz > 1) {
    std::sort(&copy[0], &copy[sz-1], lt_OptArgDesc());
  }
  
  return copy;
}


// CheckForErrors: Checks argument descriptor for errors
void
CmdLineParser::CheckForErrors(const OptArgDesc* optArgDescs)
{
  // FIXME
  //   - detect duplicate option entries.  Not pressing because
  //   FindOptDesc() will effectively do this.
  
  // Check individual descriptors
  string msg;
  string sw;
  for (const OptArgDesc* p = optArgDescs; *p != OptArgDesc_NULL; ++p) {
    // Verify that at least one switch is present
    if (p->swShort == 0 && !p->swLong) {
      throw InternalError("Arg descriptor is missing a switch!");
    }

    if (p->swLong) {
      sw = p->swLong; 
    } 
    else {
      sw = p->swShort;
    }
    
    // Verify that the kind is valid
    if (p->kind == ARG_NULL) {
      msg = "OptArgDesc.kind is invalid for: " + sw;
      throw InternalError(msg);
    }
    
    // Verify that dupKind is valid
    if (p->dupKind == DUPOPT_NULL) {
      msg = "OptArgDesc.dupKind is invalid for: " + sw;
      throw InternalError(msg);
    }
    
    // Verify that if dupKind == DUPOPT_CAT, dupArgSep is valid
    if (p->dupKind == DUPOPT_CAT && !p->dupArgSep) {
      msg = "OptArgDesc.dupArgSep is invalid for: " + sw;
      throw InternalError(msg);
    }
  }
}


// MakeSwitchDesc: Given an option string from argv (potentially
// containing both an option and an argument), create a SwDesc,
// separating switch text from any argument text.
CmdLineParser::SwDesc
CmdLineParser::MakeSwitchDesc(const char* str)
{
  // 1. Find pointers for begin/end of switch and argument
  unsigned int len = strlen(str);
  const char* strEnd = str + len;
  const char* begSw = NULL, *endSw = NULL;   // end pointers are inclusive!
  const char* begArg = NULL, *endArg = NULL;
  bool isLong = false;
  if (isLongSwitch(str)) {
    // test for --foo=arg
    begArg = strchr(str, '=');
    if (begArg) {
      begArg++;            // starts after the '='
      endArg = strEnd - 1; // ends right before '\0'
    }
    begSw = str + 2;       // bump past '--'
    endSw = (begArg) ? (begArg - 2) : (strEnd - 1);
    isLong = true;
  } 
  else if (isShortSwitch(str)) {
    // test for -f[arg]
    begArg = (len > 2) ? (str + 2) : NULL;   // starts after '-f'
    endArg = (begArg) ? (strEnd - 1) : NULL; // ends right before '\0'
    begSw  = (len > 1) ? (str + 1) : NULL;   // starts after '-'
    endSw  = begSw;                          // single character
  } 
  else {
    throw InternalError("Programming Error!");
  }
  
  // 2. Copy switch and argument substrings
  SwDesc swdesc;
  swdesc.isLong = isLong;
  for (const char* p = begSw; p && p <= endSw; ++p) { swdesc.sw += *p; }
  for (const char* p = begArg; p && p <= endArg; ++p) { swdesc.arg += *p; }
  
  return swdesc;
}


// FindOptDesc: Given a *sorted* NULL-terminated array of OptArgDesc and
// an option switch, return a reference to the appropriate OptArgDesc.
// If 'errOnMultipleMatches' is true, checks to make sure we don't
// match more than one descriptor (useful for testing long argument
// abbreviation).
const CmdLineParser::OptArgDesc*
CmdLineParser::FindOptDesc(const OptArgDesc* optArgDescs, const SwDesc& swdesc,
			   bool errOnMultipleMatches)
{
  // Note: Because there will never be very many options, we simply
  //   use a linear search.
  // Note: A long option may be a substring of another long option!
  //   Because 'optArgDescs' will be sorted, any options that are
  //   substrings of other options will be ordered so that they appear
  //   before the option that contains them, e.g. 'xx', 'xxx', 'xxxx',
  //   'xxxxx'.
  
  // Try to find a matching descriptor
  unsigned int swLen = swdesc.sw.length();
  const OptArgDesc* odesc = NULL;
  for (const OptArgDesc* p = optArgDescs; *p != OptArgDesc_NULL; ++p) {
    if (swdesc.isLong) {
      if (p->swLong && strncmp(p->swLong, swdesc.sw.c_str(), swLen) == 0) {
	odesc = p;
	break;
      }
    } 
    else {
      if (p->swShort != 0 && p->swShort == swdesc.sw[0]) {
	odesc = p;
	break;
      }
    }
  }
  if (!odesc) { return NULL; }
  
  // We have a match. Check for more matches ==> ambiguity.
  const OptArgDesc* m = NULL;
  if (errOnMultipleMatches && (m = FindOptDesc((odesc+1), swdesc, false))) {
    // Special case to handle a long option that is a substring of
    // another. If the long option switch exactly matches 'odesc' and
    // it is different than 'm' then we do not want to generate an
    // ambiguous option error.
    bool ok = (swdesc.isLong && (strcmp(odesc->swLong, swdesc.sw.c_str()) == 0)
	       && (strcmp(odesc->swLong, m->swLong) != 0));
    if (!ok) {
      string msg = "Switch `"; 
      msg += swdesc.sw; msg += "' matches two different options: ";
      if (swdesc.isLong) {
	msg += odesc->swLong; msg += ", "; msg += m->swLong;
      } 
      else {
	msg += odesc->swShort; msg += ", "; msg += m->swShort;
      }
      throw ParseError(msg);
    } 
  }
  
  return odesc;
}


// AddOption: Records the option switch and its (possibly optional)
// argument in the switch->argument map.  In order to support easy
// lookup, both the *canonical* long and short form of the switches
// are entered in the map.
void
CmdLineParser::AddOption(const OptArgDesc& odesc, const SwDesc& swdesc)
{
  if (odesc.swShort != 0) {
    string swShort(1, odesc.swShort);
    AddOption(odesc, swShort, swdesc.arg);
  }
  if (odesc.swLong) {
    string swLong(odesc.swLong);
    AddOption(odesc, swLong, swdesc.arg);
  }
}


// AddOption: Records the option switch and its (possibly optional)
// argument in the switch->argument map.  If the switch is not in the
// map, it is inserted with the available argument or NULL.  If it is
// already the map, the option descriptor defines how to handle
// duplicates.
void
CmdLineParser::AddOption(const OptArgDesc& odesc,
			 const string& sw, const string& arg)
{
  SwitchToArgMap::iterator it = switchToArgMap.find(sw);
  if (it == switchToArgMap.end()) {
    // Insert in map
    string* theArg = (arg.empty()) ? NULL : new string(arg);
    switchToArgMap.insert(SwitchToArgMap::value_type(sw, theArg));
  } 
  else {
    // Handle duplicates
    string* theArg = (*it).second;
    
    if (odesc.dupKind == DUPOPT_ERR) {
      throw ParseError("Duplicate switch: " + sw);
    }
    
    if (!arg.empty()) {
      if (!theArg) {
	theArg = new string(arg);
      } 
      else {
	if (odesc.dupKind == DUPOPT_CLOB) {
	  *theArg = arg;
	} 
	else if (odesc.dupKind == DUPOPT_CAT) {
	  *theArg += odesc.dupArgSep + arg;
	} 
      }
    }
  }
}

//****************************************************************************

