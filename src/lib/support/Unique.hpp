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

#include <include/uint.h>

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
