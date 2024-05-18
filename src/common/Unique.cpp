// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
