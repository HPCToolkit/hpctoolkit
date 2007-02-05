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

#ifndef HPCViewDocParser_h
#define HPCViewDocParser_h

//************************ System Include Files ******************************

#include <string>

//*********************** Xerces Include Files *******************************

#include <xercesc/parsers/XercesDOMParser.hpp>
using XERCES_CPP_NAMESPACE::XercesDOMParser;

#include <xercesc/dom/DOMNode.hpp> 
using XERCES_CPP_NAMESPACE::DOMNode;

//************************* User Include Files *******************************

#include <lib/prof-juicy-x/XercesErrorHandler.hpp>

#include <lib/support/diagnostics.h>

//************************ Forward Declarations ******************************

class Driver;

//****************************************************************************

class HPCViewDocParser {
public:
  HPCViewDocParser(const std::string& inputFile, 
		   HPCViewXMLErrHandler &errHndlr);
  
  ~HPCViewDocParser();

  void pass1(Driver& driver);
  void pass2(Driver& driver);

private:
  XercesDOMParser* mParser;
  DOMNode* mDoc;
};


//***************************************************************************
// 
//***************************************************************************

#define HPCViewDoc_Throw(streamArgs) DIAG_ThrowX(HPCViewDocException, streamArgs)

class HPCViewDocException : public Diagnostics::Exception {
public:
  HPCViewDocException(const std::string x,
		      const char* filenm = NULL, unsigned int lineno = 0)
    : Diagnostics::Exception(x, filenm, lineno)
    { }
  
  virtual std::string message() const { 
    return "CONFIGURATION file error [HPCViewDocException]: " + what();
  }

private:
};


#endif
