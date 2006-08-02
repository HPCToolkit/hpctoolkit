// -*-Mode: C++;-*-
// $Header$

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

#ifndef support_diagnostics_h
#define support_diagnostics_h

//************************** System Include Files ****************************

#if defined(__cplusplus)
# include <cstdlib>
#else
# include <stdlib.h>
#endif

//*************************** User Include Files *****************************

//************************** Forward Declarations ****************************

// Debug and verbosity levels: higher level --> more info; 0 turns
// respective messages off

// Private debugging level: messages for in-house debugging [0-9]
#define DIAG_DBG_LVL 0

// Public debugging level: stuff that a few users may find interesting [0-9]
extern int DIAG_DBG_LVL_PUB; // default: 0


extern "C" void 
Diagnostics_SetDiagnosticFilterLevel(int lvl);

extern "C" int
Diagnostics_GetDiagnosticFilterLevel();

// This routine is called before an error that stops execution.  It is
// especially useful for use with debuggers that do not have good
// exception support.
extern "C" void 
Diagnostics_TheMostVisitedBreakpointInHistory(const char* filenm = NULL, 
					      unsigned int lineno = 0);


//****************************************************************************
// Diagnostic macros
//****************************************************************************

// DIAG_MSG: Print a message if level satisfies the diagnostic filter

// DIAG_DEVMSG: Print a message if private level satisfies the
// private diagnostic filter

// DIAG_EMSG: Print an error message and continue.

// DIAG_ASSERT: Throw an assertion (die) if 'expr' evaluates to
// false. Stops at 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_ASSERT_WARN: Print a warning if 'expr' evaluates to false.
// Stops at 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_DIE: Print an error message and die.  Stops at
// 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_THROW: (C++ only) Throw a fatal exception.  Stops at
// 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_DIAGIF: If public diagnostic level is at least 'level' ...
#define DIAG_DIAGIF(level) if (level <= DIAG_DBG_LVL_PUB)

// DIAG_DIAGIF_DEV: If development diagnostic level is at least 'level' ...
#define DIAG_DIAGIF_DEV(level) if (level <= DIAG_DBG_LVL)

// ---------------------------------------------------------------------------
// C++ diagnostics
// ---------------------------------------------------------------------------

#if defined(__cplusplus)

#include "Exception.hpp"

#define DIAG_CERR std::cerr

// All of these macros have a parameter named 'streamArgs' for one or
// more ostream arguments. These macros use these arguments to create
// a message string.  Example:
//   if (...) DIAG_EMSG("bad val: '" << v << "'")

#define DIAG_MSG(level, streamArgs)                                 \
  if (level <= DIAG_DBG_LVL_PUB) {                                  \
    DIAG_CERR << "Diagnostics: " << streamArgs << std::endl; }

#define DIAG_DEVMSG(level, streamArgs)                              \
  if (level <= DIAG_DBG_LVL) {                                      \
    DIAG_CERR << "Diagnostics* [" << level << "]: " << streamArgs << std::endl; }

#define DIAG_EMSG(streamArgs)                                       \
  { DIAG_CERR << "error";                                           \
    if (DIAG_DBG_LVL_PUB) {                                         \
      DIAG_CERR << "[" << __FILE__ << ":" << __LINE__ << "]"; }     \
    DIAG_CERR << ": " << streamArgs << std::endl; }

#define DIAG_ASSERT(expr, streamArgs)                               \
  if (!(expr)) DIAG_THROW(streamArgs)

#define DIAG_ASSERT_WARN(expr, streamArgs)                          \
  if (!(expr)) DIAG_EMSG(streamArgs)

// (Equivalent to DIAG_THROW.)
#define DIAG_DIE(streamArgs)                                        \
  DIAG_THROW(streamArgs)

// (Equivalent to DIAG_DIE.) Based on Jean Utke's code in xaifBooster.
#define DIAG_THROW(streamArgs)                                      \
  { std::ostringstream WeIrDnAmE;                                   \
    WeIrDnAmE << streamArgs << std::ends;                           \
    throw Diagnostics::FatalException(WeIrDnAmE.str(), __FILE__, __LINE__); }

#endif


// ---------------------------------------------------------------------------
// C diagnostics
// ---------------------------------------------------------------------------

#if !defined(__cplusplus)

#include <stdio.h>

#define DIAG_MSG(level, ...)                                        \
  if (level <= DIAG_DBG_LVL_PUB) {                                  \
    fprintf(stderr, "Diagnostics [%d]: ", level);                   \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIAG_DEVMSG(level, ...)                                     \
  if (level <= DIAG_DBG_LVL) {                                      \
    fprintf(stderr, "Diagnostics* [%d]: ", level);                  \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIAG_EMSG(...)                                              \
  { fputs("error", stderr);                                         \
    if (DIAG_DBG_LVL_PUB) {                                         \
      fprintf(stderr, " [%s:%d]", __FILE__, __LINE__); }            \
    fputs(": ", stderr); fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

//#define DIAG_ASSERT(expr, ...) // cf. Open64's FmtAssert

//#define DIAG_ASSERT_WARN(expr, ...)

#define DIAG_DIE(...)                                               \
  DIAG_EMSG(__VA_ARGS__);                                           \
  { Diagnostics_TheMostVisitedBreakpointInHistory(__FILE__, __LINE__); exit(1); }

#endif


//****************************************************************************
// 
//****************************************************************************

extern const char* DIAG_UNIMPLEMENTED;
extern const char* DIAG_UNEXPECTED_INPUT;
extern const char* DIAG_UNEXPECTED_OPR;


#endif /* support_diagnostics_h */

