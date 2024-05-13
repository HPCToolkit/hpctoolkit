// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// **************************************************************************
//
// File:
//    Unique.h
//
// Purpose:
//    Defines the Unique class (see below).
//
// Description:
//    Mixin for non-copyable objects or singleton objects.
//    All class definitions should contain copy constructors and assignment
//    operators.  Many class's objects, however, should never be copied.
//    If this is the case, the class should multiply inherit from the
//    Unique mixin class, which inhibits these operations.  The Unique
//    class additionally provides a mechanism for classes that should only
//    have single objects (a stronger form of a non-copyable object).
//    In this later case, the second constructor should be used and the
//    class name specified as a (const char*) string.
//
//    Note that for Unique objects, object equality amounts to pointer
//    equality, so equality and inequality operators can be easily defined.
//
// **************************************************************************

#ifndef support_Unique_h
#define support_Unique_h


// ************************* System Include Files ***************************

#include <set>
#include <string>

// ************************** User Include Files ****************************


// ************************* Variable Definitions ***************************

// *************************** Class Definitions ****************************

// **************************************************************************
//
//  Class:
//     Unique
//
//  Class Data Members:
//     std::string className private -- the name of the class, if singleton.
//
//  Purpose:
//     1) Mixin for a class with objects that shouldn't be copied.
//     2) Mixin for a class which should have only a single object.
//
//  History:
//     09/24/96 - dbaker - Creation (adapted from Memento).
//
// **************************************************************************
class Unique
{
  protected:

    // Special Class Members

      Unique();
        // Default constructor.

      Unique(const char* theClassName);
        // Constructor for singleton object.

      virtual ~Unique();
        // Destructor.


  private:

      static std::set<std::string> classNameSet;
        // Where the set of class names are stored.

      Unique(const Unique& mo);
        // Copy constructor (explicitly not allowed).
        // has not implemetationm since compiler should complain
        // about inaccessability
      Unique& operator=(const Unique& mo);
        // Assignment operator (explicitly not allowed).
        // has not implemetationm, ...

      std::string className;
        // Saved class name or ""

};

// ********************** Extern Function Prototypes ************************

// Unique object equality.
inline bool operator==(const Unique& u1, const Unique& u2)
{
    return (&u1 == &u2);
}


// Unique object inequality.
inline bool operator!=(const Unique& u1, const Unique& u2)
{
    return !(u1 == u2);
}


#endif /* support_Unique_h */
