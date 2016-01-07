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

#ifndef support_Logic_hpp
#define support_Logic_hpp

//************************** System Include Files ****************************

#include <iostream>
#include <fstream>
#include <string>

#include <inttypes.h>

//*************************** User Include Files *****************************

#include <include/uint.h>

//************************** Forward Declarations ****************************

//****************************************************************************
// Logic
//****************************************************************************

namespace Logic {

// --------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------

// equiv: returns (p <-> q)
//   p <-> q == (p && q) || (!p && !q)
inline bool
equiv(bool p, bool q)
{
  return ((p && q) || (!p && !q));
}

// declaration to remove Intel compiler warning
template <typename T>
bool
equiv(T p, T q);

template <typename T>
bool
equiv(T p, T q)
{
  return ((p && q) || (!p && !q));
}


// implies: returns (p -> q)
//   p -> q == !p || q
inline bool
implies(bool p, bool q)
{
  return (!p || q);
}

// declaration to remove Intel compiler warning
template <typename T>
bool
implies(T p, T q);

template <typename T>
bool
implies(T p, T q)
{
  return (!p || q);
}

#if 0
// Would it be better to have a specialization of the template, like so?
template <>
inline bool
implies<bool>(bool p, bool q)
{
  return (!p || q);
}
#endif


} // namespace Logic


#endif // support_Logic_hpp
