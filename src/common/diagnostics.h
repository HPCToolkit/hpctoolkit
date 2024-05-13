// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#if defined(__cplusplus)
#define DIAG_EXTERN extern "C"
#else
#define DIAG_EXTERN extern
#endif

// Debug and verbosity levels: higher level --> more info; 0 turns
// respective messages off

// Private debugging level: messages for in-house debugging [0-9]
#define DIAG_DBG_LVL 0

// Public debugging level: stuff that a few users may find interesting [0-9]
extern int DIAG_DBG_LVL_PUB; // default: 0

DIAG_EXTERN void
Diagnostics_SetDiagnosticFilterLevel(int lvl);

DIAG_EXTERN int
Diagnostics_GetDiagnosticFilterLevel();

// This routine is called before an error that stops execution.  It is
// especially useful for use with debuggers that do not have good
// exception support.
DIAG_EXTERN void
Diagnostics_TheMostVisitedBreakpointInHistory(const char* filenm,
                                              unsigned int lineno);


//****************************************************************************
// Diagnostic macros
//****************************************************************************

// DIAG_MsgIf: Print a message if the expression is true

// DIAG_Msg: Print a message if level satisfies the diagnostic filter

// DIAG_DevMsgIf: Print a message if the expression is true

// DIAG_DevMsg: Print a message if private level satisfies the
// private diagnostic filter

// DIAG_EMsg: Print an error message and continue.

// DIAG_Assert: Throw an assertion (die) if 'expr' evaluates to
// false. Stops at 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_AssertWarn: Print a warning if 'expr' evaluates to false.

// DIAG_Die: Print an error message and die.  Stops at
// 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_Throw: (C++ only) Throw a fatal exception.  Stops at
// 'Diagnostics_TheMostVisitedBreakpointInHistory'.

// DIAG_If: If public diagnostic level is at least 'level' ...
#define DIAG_If(level) if (level <= DIAG_DBG_LVL_PUB)

// DIAG_DevIf: If development diagnostic level is at least 'level' ...
#define DIAG_DevIf(level) if (level <= DIAG_DBG_LVL)

// ---------------------------------------------------------------------------
// C++ diagnostics
// ---------------------------------------------------------------------------

#if defined(__cplusplus)

#include <sstream>

#include "Exception.hpp"

#define DIAG_CERR std::cerr
#define DIAG_ENDL std::endl /*<< std::flush*/

// All of these macros have a parameter named 'streamArgs' for one or
// more ostream arguments. These macros use these arguments to create
// a message string.  Example:
//   if (...) DIAG_EMsg("bad val: '" << v << "'")

#define DIAG_MsgIf_GENERIC(tag, ifexpr, streamArgs)                 \
  if (ifexpr) {                                                     \
    DIAG_CERR << tag << streamArgs << DIAG_ENDL; }


#define DIAG_MsgIf(ifexpr, streamArgs)                              \
  DIAG_MsgIf_GENERIC("msg: ", ifexpr, streamArgs)

#define DIAG_MsgIfCtd(ifexpr, streamArgs)                           \
  DIAG_MsgIf_GENERIC("", ifexpr, streamArgs)

#define DIAG_Msg(level, streamArgs)                                 \
  DIAG_MsgIf((level <= DIAG_DBG_LVL_PUB), streamArgs)

#define DIAG_MsgCtd(level, streamArgs)                              \
  DIAG_MsgIfCtd((level <= DIAG_DBG_LVL_PUB), streamArgs)


#define DIAG_DevMsgIf(ifexpr, streamArgs)                           \
  DIAG_MsgIf_GENERIC("msg*: ", ifexpr, streamArgs)

#define DIAG_DevMsgIfCtd(ifexpr, streamArgs)                        \
  DIAG_MsgIf_GENERIC("", ifexpr, streamArgs)

#define DIAG_DevMsg(level, streamArgs)                              \
  if (level <= DIAG_DBG_LVL) {                                      \
    DIAG_CERR << "msg* [" << level << "]: " << streamArgs << DIAG_ENDL; }


#define DIAG_WMsgIf(ifexpr, streamArgs)                              \
  DIAG_MsgIf_GENERIC("WARNING: ", ifexpr, streamArgs)

#define DIAG_WMsgIfCtd(ifexpr, streamArgs)                           \
  DIAG_MsgIf_GENERIC("", ifexpr, streamArgs)

#define DIAG_WMsg(level, streamArgs)                                 \
  DIAG_WMsgIf((level <= DIAG_DBG_LVL_PUB), streamArgs)

#define DIAG_EMsg(streamArgs)                                       \
  DIAG_CERR << "ERROR: " << streamArgs << DIAG_ENDL

#define DIAG_DevEMsg(streamArgs)                                       \
  { DIAG_CERR << "ERROR: " << streamArgs << DIAG_ENDL;              \
    if (DIAG_DBG_LVL_PUB) {                                         \
      DIAG_CERR << "\t[" << __FILE__ << ":" << __LINE__ << "]" << DIAG_ENDL; } \
  }

#define DIAG_Assert(expr, streamArgs)                               \
  if (!(expr)) DIAG_Throw(streamArgs)

#define DIAG_AssertWarn(expr, streamArgs)                           \
  if (!(expr)) DIAG_EMsg(streamArgs)

// (Equivalent to DIAG_Throw.)
#define DIAG_Die(streamArgs)                                        \
  DIAG_Throw(streamArgs)

// (Equivalent to DIAG_Die.) Based on Jean Utke's code in xaifBooster.
#define DIAG_Throw(streamArgs)                                      \
  { std::ostringstream WeIrDnAmE;                                   \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                       \
    throw Diagnostics::FatalException(WeIrDnAmE.str(), __FILE__, __LINE__); }

#define DIAG_ThrowX(ExceptionClass, streamArgs)                     \
  { std::ostringstream WeIrDnAmE;                                   \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                       \
    throw ExceptionClass(WeIrDnAmE.str(), __FILE__, __LINE__); }

#endif


// ---------------------------------------------------------------------------
// C diagnostics
// ---------------------------------------------------------------------------

#if !defined(__cplusplus)

#include <stdio.h>

#define DIAG_MsgIf(ifexpr, ...)                                     \
  if (ifexpr) {                                                     \
    fputs("msg: ", stderr);                                         \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIAG_Msg(level, ...)                                        \
  if (level <= DIAG_DBG_LVL_PUB) {                                  \
    fprintf(stderr, "msg [%d]: ", level);                   \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIAG_DevMsg(level, ...)                                     \
  if (level <= DIAG_DBG_LVL) {                                      \
    fprintf(stderr, "msg* [%d]: ", level);                  \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIAG_EMsg(...)                                              \
  { fputs("ERROR: ", stderr);                                       \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr);              \
  }

#define DIAG_DevEMsg(...)                                              \
  { fputs("ERROR: ", stderr);                                       \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr);              \
    if (DIAG_DBG_LVL_PUB) {                                         \
      fprintf(stderr, "\t[%s:%d]\n", __FILE__, __LINE__); }         \
  }

//#define DIAG_Assert(expr, ...) // cf. Open64's FmtAssert

//#define DIAG_AssertWarn(expr, ...)

#define DIAG_Die(...)                                               \
  DIAG_EMsg(__VA_ARGS__);                                           \
  { Diagnostics_TheMostVisitedBreakpointInHistory(__FILE__, __LINE__); exit(1); }

#endif


//****************************************************************************
//
//****************************************************************************

extern const char* DIAG_Unimplemented;
extern const char* DIAG_UnexpectedInput;
extern const char* DIAG_UnexpectedOpr;


#endif /* support_diagnostics_h */
