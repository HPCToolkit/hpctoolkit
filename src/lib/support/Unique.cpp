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

// ***************************************************************************
//
// File:
//    Unique.C
//
// Purpose:
//    Implements Unique class.
//
// Description:
//    See Unique.h.
//
// History:
//    09/24/96 - dbaker - Creation (adapted from Memento).
//
// ***************************************************************************

// ************************* System Include Files ***************************

// *************************** User Include Files ****************************

#include "Unique.hpp"
#include "diagnostics.h"

// ************************** Variable Definitions ***************************

std::set<std::string> Unique::classNameSet;  // Set of saved class names.


// ***************************************************************************
//
//  Class method:
//     Unique::Unique protected (constructor)
//
//  Purpose:
//     Constructor for normal non-copyable object.
//
// ***************************************************************************

Unique::Unique(): className()
{
}



// ***************************************************************************
//
//  Class method:
//     Unique::Unique protected (constructor)
//
//  Parameters:
//     const char* theClassName -- the name of the class, presumably unique.
//
//  Purpose:
//     Constructor for singleton object.
//
// ***************************************************************************

Unique::Unique(const char* theClassName)
  : className((theClassName) ? theClassName : "")
{
  if (classNameSet.count(className) != 0) { 
    DIAG_Die("Trying to create another " + className + " instance");
  }
  else {
    classNameSet.insert(className); 
  }
}


// ***************************************************************************
//
//  Class method:
//     Unique::~Unique public (destructor)
//
//  Parameters:
//     none
//
//  Purpose:
//     Destroys the object.
//
//  History:
//     09/24/96 - dbaker - Creation (adapted from Memento).
//
// ***************************************************************************

Unique::~Unique()
{
    if (className.length() > 0)
    {// remove the className from the master set
        classNameSet.erase(className);
    }
}
