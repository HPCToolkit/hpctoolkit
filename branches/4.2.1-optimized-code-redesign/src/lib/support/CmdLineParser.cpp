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

//************************** Open64 Include Files ***************************

//*************************** User Include Files ****************************

#include "CmdLineParser.hpp"

//*************************** Forward Declarations ***************************

using std::string;

static string MISSING_SWITCH = "Missing switch after -";
static string UNKNOWN_SWITCH = "Unknown option switch: ";
static string MISSING_ARG = "Missing argument for switch: ";

//****************************************************************************

// IsDashDash
static inline bool
IsDashDash(const char* str) { return (strcmp(str, "--") == 0); }


// IsSwitch, IsLongSwitch, IsShortSwitch: Assumes str is non-NULL!  Note also
// that the test for short switch is not quite complete and depends on
// testing for a long switch first!
static inline bool
IsLongSwitch(const char* str) { return (strncmp(str, "--", 2) == 0); }

static inline bool
IsShortSwitch(const char* str) { return (*str == '-'); }

static inline bool
IsSwitch(const char* str) { return (IsLongSwitch(str) || IsShortSwitch(str)); }


// IsArg: Verifies that we should interpret 'str' as an argument.
// Should be non-NULL;
static inline bool
IsArg(const char* str) { return (!IsSwitch(str) && !IsDashDash(str)); }


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
  Parse(optArgDescs, argc, argv);
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
CmdLineParser::Parse(const OptArgDesc* optArgDescs, 
		     int argc, const char* const argv[])
{ 
  Reset();
  command = argv[0]; // always do first so it will be available after errors
  
  CheckForErrors(optArgDescs);
  
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
    if (IsDashDash(str)) {
      endOfOpts = true;
      continue;
    }
    
    if (!endOfOpts && IsSwitch(str)) {
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
      } else if (d->kind == ARG_REQ || d->kind == ARG_OPT) {
	if (swdesc.arg.empty()) {
	  int nexti = i + 1;
	  if (nexti < argc && argv[nexti] && IsArg(argv[nexti])) {
	    swdesc.arg = argv[nexti];
	    i = nexti; // increment iteration
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
}


//****************************************************************************

const string& 
CmdLineParser::GetCmd() const
{
  return command;
}


// IsOpt:
bool 
CmdLineParser::IsOpt(const char swShort) const
{
  string sw(1, swShort);
  return IsOpt(sw);
}

bool 
CmdLineParser::IsOpt(const char* swLong) const
{
  string sw(swLong);
  return IsOpt(sw);
}

bool 
CmdLineParser::IsOpt(const string& sw) const
{
  SwitchToArgMap::const_iterator it = switchToArgMap.find(sw);
  return (it != switchToArgMap.end());
}


// IsOptArg:
bool 
CmdLineParser::IsOptArg(const char swShort) const
{
  string sw(1, swShort);
  return IsOptArg(sw);
}

bool 
CmdLineParser::IsOptArg(const char* swLong) const
{
  string sw(swLong);
  return IsOptArg(sw);
}

bool 
CmdLineParser::IsOptArg(const string& sw) const
{
  SwitchToArgMap::const_iterator it = switchToArgMap.find(sw);
  if ((it != switchToArgMap.end()) && ((*it).second != NULL)) {
    return true;
  }
  return false;
}


// GetOptArg:
const string&
CmdLineParser::GetOptArg(const char swShort) const
{
  string sw(1, swShort);
  return GetOptArg(sw);
}

const string&
CmdLineParser::GetOptArg(const char* swLong) const
{
  string sw(swLong);
  return GetOptArg(sw);
}

const string&
CmdLineParser::GetOptArg(const string& sw) const
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
CmdLineParser::GetNumArgs() const
{ 
  return arguments.size(); 
}

const string& 
CmdLineParser::GetArg(unsigned int i) const
{
  return arguments[i];
}


//****************************************************************************

long
CmdLineParser::ToLong(const string& str)
{
  long value = 0;
  if (str.empty()) { throw InternalError("ToLong"); }
  
  errno = 0;
  char* endptr = NULL;
  value = strtol(str.c_str(), &endptr, 0);
  if (errno || (endptr && strlen(endptr) > 0)) {
    string msg = "Argument `" + str 
      + "' cannot be converted to integral value.";
    if (errno) { // not always set
      msg += " ";
      msg += strerror(errno);
    }
    throw ParseError(msg);
  } 
  return value;
}


uint64_t
CmdLineParser::ToUInt64(const string& str)
{
  uint64_t value = 0;
  if (str.empty()) { throw InternalError("ToUInt64"); }
  
  errno = 0;
  char* endptr = NULL;
  value = strtoul(str.c_str(), &endptr, 0);
  if (errno || (endptr && strlen(endptr) > 0)) {
    string msg = "Argument `" + str 
      + " cannot be converted to integral value.";
    if (errno) { // not always set
      msg += " ";
      msg += strerror(errno);
    }
    throw ParseError(msg);
  } 
  return value;
}


double   
CmdLineParser::ToDbl(const string& str)
{
  double value = 0;
  if (str.empty()) { throw InternalError("ToDbl"); }
  
  errno = 0;
  char* endptr = NULL;
  value = strtod(str.c_str(), &endptr);
  if (errno || (endptr && strlen(endptr) > 0)) {
    string msg = "Argument `" + str + "' cannot be converted to real value.";
    if (errno) { // not always set
      msg += " ";
      msg += strerror(errno);
    }
    throw ParseError(msg);
  } 
  return value;
}


//****************************************************************************

void 
CmdLineParser::Dump(std::ostream& os) const
{
  os << "Command: `" << GetCmd() << "'" << std::endl;
  
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
CmdLineParser::DDump() const
{
  Dump(std::cerr);
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
    } else {
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
  if (IsLongSwitch(str)) {
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
  else if (IsShortSwitch(str)) {
    // test for -f[arg]
    begArg = (len > 2) ? (str + 2) : NULL;   // starts after '-f'
    endArg = (begArg) ? (strEnd - 1) : NULL; // ends right before '\0'
    begSw  = (len > 1) ? (str + 1) : NULL;   // starts after '-'
    endSw  = begSw;                               // single character
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


// FindOptDesc: Given a NULL-terminated array of OptArgDesc and an
// option switch, return a reference to the appropriate OptArgDesc.
// If 'errOnMultipleMatches' is true, checks to make sure we don't
// match more than one descriptor (useful for testing long argument
// abbreviation).
const CmdLineParser::OptArgDesc*
CmdLineParser::FindOptDesc(const OptArgDesc* optArgDescs, const SwDesc& swdesc,
			   bool errOnMultipleMatches)
{
  // Note: Because there will never be very many options, we simply
  // use a linear search.
  
  // Try to find a matching descriptor
  unsigned int swLen = swdesc.sw.length();
  const OptArgDesc* odesc = NULL;
  for (const OptArgDesc* p = optArgDescs; *p != OptArgDesc_NULL; ++p) {
    if (swdesc.isLong) {
      if (p->swLong && strncmp(p->swLong, swdesc.sw.c_str(), swLen) == 0) {
	odesc = p;
	break;
      }
    } else {
      if (p->swShort != 0 && p->swShort == swdesc.sw[0]) {
	odesc = p;
	break;
      }
    }
  }
  if (!odesc) { return NULL; }
  
  // We have a match. Check for more matches.
  const OptArgDesc* m = NULL;
  if (errOnMultipleMatches && (m = FindOptDesc((odesc+1), swdesc, false))) {
    string msg = "Switch `"; 
    msg += swdesc.sw; msg += "' matches two different options: ";
    if (swdesc.isLong) {
      msg += odesc->swLong; msg += ", "; msg += m->swLong;
    } else {
      msg += odesc->swShort; msg += ", "; msg += m->swShort;
    }
    throw ParseError(msg);
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
  } else {
    // Handle duplicates
    string* theArg = (*it).second;
    
    if (odesc.dupKind == DUPOPT_ERR) {
      throw ParseError("Duplicate switch: " + sw);
    }
    
    if (!arg.empty()) {
      if (!theArg) {
	theArg = new string(arg);
      } else {
	if (odesc.dupKind == DUPOPT_CLOB) {
	  *theArg = arg;
	} else if (odesc.dupKind == DUPOPT_CAT) {
	  *theArg += odesc.dupArgSep + arg;
	} 
      }
    }
  }
}

//****************************************************************************

