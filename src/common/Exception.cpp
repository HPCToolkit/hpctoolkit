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

//*************************** User Include Files *****************************

#include "Exception.hpp"
#include "diagnostics.h"

//************************** Forward Declarations ****************************

//****************************************************************************
// Exception
//****************************************************************************

Diagnostics::Exception::Exception(const char* x,
                                  const char* filenm, unsigned int lineno)
{
  std::string str = x;
  Ctor(str, filenm, lineno);
  Diagnostics_TheMostVisitedBreakpointInHistory(filenm, lineno);
}


Diagnostics::Exception::Exception(const std::string x,
                                  const char* filenm, unsigned int lineno)
{
  Ctor(x, filenm, lineno);
  Diagnostics_TheMostVisitedBreakpointInHistory(filenm, lineno);
}


Diagnostics::Exception::~Exception()
{
}


void
Diagnostics::Exception::Ctor(const std::string& x,
                             const char* filenm, unsigned int lineno)
{
  mWhat = x;
  if (filenm && lineno != 0) {
    std::ostringstream os;
    os << filenm << ":" << lineno;
    mWhere = os.str();
  }
}

//****************************************************************************
// FatalException
//****************************************************************************

Diagnostics::FatalException::FatalException(const char* x,
                                            const char* filenm,
                                            unsigned int lineno)
  : Diagnostics::Exception(x, filenm, lineno)
{
}


Diagnostics::FatalException::FatalException(const std::string x,
                                            const char* filenm,
                                            unsigned int lineno)
  : Diagnostics::Exception(x, filenm, lineno)
{
}


Diagnostics::FatalException::~FatalException()
{
}
