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

#ifndef support_Exception_hpp
#define support_Exception_hpp

//************************** System Include Files ****************************

#include <iostream>
#include <sstream>
#include <string>

//*************************** User Include Files *****************************

//************************** Forward Declarations ****************************

//****************************************************************************

// Note: Use the interface in "diagnostics.h"

//****************************************************************************
// Exception
//****************************************************************************

namespace Diagnostics {

  class BaseException {
  public:
    // -------------------------------------------------------
    // constructor/destructor
    // -------------------------------------------------------
    BaseException() { }

    virtual ~BaseException() { }

    // -------------------------------------------------------
    // message/reporting
    // -------------------------------------------------------
    // what: what the exception was about (cv. std::exception)
    virtual const std::string& what() const = 0;

    // message: a reporting message
    virtual std::string message() const = 0;

    // report: generate message using 'message'
    virtual void report(std::ostream& os) const = 0;
    virtual void report() const = 0;
  };

  // A generic Diagnostics exception with file/line information
  class Exception : public BaseException {
  public:
    // -------------------------------------------------------
    // constructor/destructor
    // -------------------------------------------------------
    Exception(const char* x,
              const char* filenm = NULL, unsigned int lineno = 0);

    Exception(const std::string x,
              const char* filenm = NULL, unsigned int lineno = 0);

    virtual ~Exception();

    // -------------------------------------------------------
    // what/where
    // -------------------------------------------------------
    virtual const std::string& what() const { return mWhat; }

    // where: where (in the source code) the exception was raised
    virtual const std::string& where() const { return mWhere; }

    // -------------------------------------------------------
    // message/reporting
    // -------------------------------------------------------
    virtual std::string message() const {
      return "[Diagnostics::Exception] " + mWhat + " (" + mWhere + ")";
    }

    virtual void report(std::ostream& os) const {
      os << message() << std::endl;
    }

    virtual void report() const { report(std::cerr); }

  protected:
    void Ctor(const std::string& x,
              const char* filenm = NULL, unsigned int lineno = 0);

    std::string mWhat;
    std::string mWhere;
  };

  // A fatal Diagnostics exception that generally should be unrecoverable
  class FatalException : public Exception {
  public:
    // -------------------------------------------------------
    // constructor/destructor
    // -------------------------------------------------------
    FatalException(const char* x,
                   const char* filenm = NULL, unsigned int lineno = 0);

    FatalException(const std::string x,
                   const char* filenm = NULL, unsigned int lineno = 0);

    virtual ~FatalException();

    // -------------------------------------------------------
    // message/reporting
    // -------------------------------------------------------
    virtual std::string message() const {
      return "[Diagnostics::FatalException] " + mWhat + " [" + mWhere + "]";
    }

  };

} /* namespace Diagnostics */

//****************************************************************************

#endif // support_Exception_hpp
