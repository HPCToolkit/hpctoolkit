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

//************************* System Include Files ****************************

#include <string>
using std::string;


//*************************** User Include Files ****************************

#include "ProcNameMgr.hpp"

#include "diagnostics.h"


//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// ProcNameMgr
//***************************************************************************

// Canonicalize C++ templates and overloading: Compute a 'generic'
// name for a templated function type:
//   f<...>            		       --> f<>
//   f<...>()          		       --> f<>()
//   f<...>(T<...>* x) 		       --> f<>(T<>* x)
//   f<...>::foo()     		       --> f<>::foo()
//   f<resultT(objT::*)(a1,a2)>::foo() --> f<>::foo()
//   f(T<...>* x)      		       --> f(T<>* x)
//
// Be careful!
//   operator<<()      --> (no change)
//   operator>>()      --> (no change)

// Invariants about function names:
//   Templates may have multiple nested < >, but have at least one pair
//   Excluding function pointer types, functions have at most one pair of ( )

std::string 
ProcNameMgr::canonicalizeCppTemplate(const std::string& name)
{
  size_t posLangle = name.find_first_of('<'); // returns valid pos or npos

  // Ensure we have a name of the form: "...<..." but not "...<<..."
  if ((0 < posLangle && posLangle < (name.length() - 1))
      && name[posLangle+1] != '<') {

    string x = name.substr(0, posLangle /*len*/); // exclude '<'

    int nesting = 0;
    for (uint i = posLangle; i < name.size(); i++) {
      char c = name[i];

      bool save_c = (nesting == 0);
      if (c == '<') {
	nesting++;
      }
      else if (c == '>') {
	nesting--;
      }
      save_c = (save_c || (nesting == 0));

      if (save_c) {
	x += c;
      }
    }
    
    return x;
  }
  
  return name;
}


//***************************************************************************
// CilkNameMgr
//***************************************************************************

const string CilkNameMgr::cilkmain = "cilk_main";

// Cilk creates four specializations of each procedure:
//   fast:    <x>
//   slow:   _cilk_<x>_slow
//   import: _cilk_<x>_import
//   export: mt_<x>

const string CilkNameMgr::s_procSlow_pfx   = "_cilk_";
const string CilkNameMgr::s_procSlow_sfx   = "_slow";

const string CilkNameMgr::s_procImport_pfx = "_cilk_";
const string CilkNameMgr::s_procImport_sfx = "_import";

const string CilkNameMgr::s_procExport_pfx = "mt_";
const string CilkNameMgr::s_procExport_sfx = "";


// Cilk 'outlines' an inlet <x> within a procedure <proc>, creating
// three specializations:
//   norm: _cilk_<proc>_<x>_inlet
//   fast: _cilk_<proc>_<x>_inlet_fast
//   slow: _cilk_<proc>_<x>_inlet_slow

const string CilkNameMgr::s_inletNorm_pfx = "_cilk_";
const string CilkNameMgr::s_inletNorm_sfx = "_inlet";

const string CilkNameMgr::s_inletFast_pfx = "_cilk_";
const string CilkNameMgr::s_inletFast_sfx = "_inlet_fast";

const string CilkNameMgr::s_inletSlow_pfx = "_cilk_";
const string CilkNameMgr::s_inletSlow_sfx = "_inlet_slow";


string
CilkNameMgr::canonicalize(const string& name)
{
  // ------------------------------------------------------------
  // inlets: must test before procedures because of _slow suffix
  // ------------------------------------------------------------
  if (isGenerated(name, s_inletNorm_pfx, s_inletNorm_sfx)) {
    return basename(name, s_inletNorm_pfx, s_inletNorm_sfx);
  }
  else if (isGenerated(name, s_inletFast_pfx, s_inletFast_sfx)) {
    return basename(name, s_inletFast_pfx, s_inletFast_sfx);
  }
  else if (isGenerated(name, s_inletSlow_pfx, s_inletSlow_sfx)) {
    return basename(name, s_inletSlow_pfx, s_inletSlow_sfx);
  }

  // ------------------------------------------------------------
  // procedures
  // ------------------------------------------------------------
  else if (isGenerated(name, s_procSlow_pfx, s_procSlow_sfx)) {
    return basename(name, s_procSlow_pfx, s_procSlow_sfx);
  }
  else if (isGenerated(name, s_procImport_pfx, s_procImport_sfx)) {
    return basename(name, s_procImport_pfx, s_procImport_sfx);
  }
  else if (isGenerated(name, s_procExport_pfx, s_procExport_sfx)) {
    return basename(name, s_procExport_pfx, s_procExport_sfx);
  }

  // ------------------------------------------------------------
  // special case
  // ------------------------------------------------------------
  else if (name == "_cilk_cilk_main_import") {
    // _cilk_cilk_main_import --> invoke_main_slow
    return "invoke_main_slow";
  }

  // ------------------------------------------------------------
  // default
  // ------------------------------------------------------------
  else {
    return name;
  }
}


//***************************************************************************


