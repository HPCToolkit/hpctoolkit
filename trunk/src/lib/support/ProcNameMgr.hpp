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
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ProcNameMgr_hpp 
#define ProcNameMgr_hpp

//************************* System Include Files ****************************

#include <string>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

//*************************** Forward Declarations **************************

//***************************************************************************
// ProcNameMgr
//***************************************************************************

// --------------------------------------------------------------------------
// 'ProcNameMgr' 
// --------------------------------------------------------------------------

class ProcNameMgr
{
public:
  ProcNameMgr() { }
  virtual ~ProcNameMgr() { }

  virtual std::string 
  canonicalize(const std::string& name) = 0;

};


// --------------------------------------------------------------------------
// 'CilkNameMgr' 
// --------------------------------------------------------------------------

class CilkNameMgr : public ProcNameMgr
{
public:
  CilkNameMgr() { }
  virtual ~CilkNameMgr() { }

  virtual std::string 
  canonicalize(const std::string& name);

private:

  bool 
  is_slow_proc(const std::string& x)
  {
    return (x.compare(0, s_slow_pfx.length(), s_slow_pfx) == 0);
  }

private:
  static const std::string s_slow_pfx;
  static const std::string s_slow_sfx;
};


//***************************************************************************

#endif // ProcNameMgr_hpp
