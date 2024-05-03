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

#ifndef support_Logic_hpp
#define support_Logic_hpp

//************************** System Include Files ****************************

#include <iostream>
#include <fstream>
#include <string>

#include <inttypes.h>

//*************************** User Include Files *****************************


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


} // namespace Logic


#endif // support_Logic_hpp
