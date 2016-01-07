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

#ifndef support_Exception_hpp 
#define support_Exception_hpp

//************************** System Include Files ****************************

#include <iostream>
#include <sstream>
#include <string>

//*************************** User Include Files *****************************

//************************** Forward Declarations ****************************

//****************************************************************************

// Note: Use the inteface in "diagnostics.h"

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
