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
// Copyright ((c)) 2002-2019, Rice University
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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef BinUtil_Dbg_Proc_hpp
#define BinUtil_Dbg_Proc_hpp

//************************* System Include Files ****************************

#include <string>
#include <deque>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "VMAInterval.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/support/SrcFile.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// LM (LoadModule)
//***************************************************************************

namespace BinUtil {

namespace Dbg {

// --------------------------------------------------------------------------
// 'Proc' represents debug information for a procedure
// --------------------------------------------------------------------------

class Proc {
public:
  Proc() 
    : parent(NULL), parentVMA(0),
      begVMA(0), endVMA(0), name(""), filenm(""), begLine(0)
  { }
  ~Proc() { }

  Proc& operator=(const Proc& x) 
  {
    if (this != &x) {
      parent    = x.parent;
      parentVMA = x.parentVMA;
      begVMA    = x.begVMA;
      endVMA    = x.endVMA;
      name      = x.name;
      filenm    = x.filenm;
      begLine   = x.begLine;
    }
    return *this;
  }
  

  // private:
  Proc* parent;
  VMA   parentVMA;
  
  VMA begVMA; // begin VMA
  VMA endVMA; // end VMA (at the end of the last insn)
  std::string name, filenm;
  SrcFile::ln begLine;
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::string toString() const;

  std::ostream& dump(std::ostream& os) const;

  void ddump() const;

private:

};

  
} // namespace Dbg

} // namespace BinUtil

//***************************************************************************

#endif // BinUtil_Dbg_Proc_hpp
